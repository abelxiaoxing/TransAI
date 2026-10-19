// Bundled OCR backend: PP-OCRv3 (Chinese + English) detection + recognition on
// ncnn, running inside the application process.
//
// Why this backend exists: the tesseract CLI backend depends on whatever the
// user happens to have installed (often nothing, and almost never the Chinese
// language pack). The ncnn route ships ~13 MB of models with the app, has no
// external process, and is markedly better on screenshots (UI text, mixed
// Chinese/English).
//
// Deliberate simplifications versus the upstream RapidOcrNcnn reference
// implementation:
//   * no OpenCV — boxes come from 8-connected component labelling on the
//     score map (see OcrPostProcess.h), so no clipper/polygon code either;
//   * no angle-classifier model (~0.5 MB + one inference per line): screenshot
//     text is horizontal, and the crops are axis-aligned;
//   * detection is capped to a 960 px long side, recognition to 48 px height.

#include "ocr/OcrEngine.h"
#include "ocr/OcrModelLocator.h"
#include "ocr/OcrPostProcess.h"

#include <QDebug>
#include <QFile>
#include <QFuture>
#include <QFutureWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <memory>

#include "net.h"

namespace {

// Detection/recognition geometry. 960 px matches PaddleOCR's default
// det_limit_side_len; 48 px is the height the PP-OCR recogniser was trained at.
constexpr int kDetTargetSize = 960;
constexpr int kDetSizeAlignment = 32;
constexpr int kRecHeight = 48;
// A single recognition pass is O(width); very long lines are capped so a
// pathological crop cannot stall the worker for seconds.
constexpr int kMaxRecWidth = 2048;
// Mean character confidence below which a line is treated as noise.
constexpr float kMinLineScore = 0.3f;
constexpr int kMinCropHeight = 4;
// Ceiling for ncnn's intra-op parallelism (net.opt.num_threads drives the
// `#pragma omp parallel for num_threads(...)` in ncnn's kernels). Measured on an
// i5-1035G4 (4C/8T): 2 threads 0.78s, 4 threads 0.55s, 8 threads 0.64s for a
// 12-line 1080p screenshot - hyperthreads beyond the physical cores only add
// oversubscription, so 4 is the ceiling regardless of how wide the CPU is.
constexpr int kMaxThreads = 4;

struct OcrRunResult {
    QString text;
    QString error;
    QString loadError;  // models exist but failed to load (engine is then dead)
};

// ncnn's Net is not thread-safe; one inference at a time is also plenty for a
// screenshot-translation tool.
struct Nets {
    ncnn::Net det;
    ncnn::Net rec;
};

}  // namespace

class RapidOcrEngine : public OcrEngine {
public:
    explicit RapidOcrEngine(QObject* parent = nullptr)
        : OcrEngine(parent)
        , m_paths(OcrModelLocator::locate())
        , m_numThreads(std::clamp(QThread::idealThreadCount(), 1, kMaxThreads)) {
        // Nothing asynchronous here: the locator only stats a handful of
        // files, so availability is known before the first frame. A set of
        // models that exists but fails to load flips this back to false later
        // (see the QFutureWatcher handler in recognize()).
        m_available = m_paths.isValid();
        if (!m_paths.isValid()) {
            // Fixed English sentence: it doubles as an I18n table key. The
            // searched directories only go to the log.
            m_unavailableReason = QStringLiteral("OCR is not available: the bundled OCR models are missing");
            qWarning() << m_unavailableReason << "- searched" << OcrModelLocator::candidateDirectories();
        }
    }

    ~RapidOcrEngine() override {
        // The worker captures `this`; never let it outlive the engine.
        m_future.waitForFinished();
    }

    bool isAvailable() override { return m_available; }
    QString unavailableReason() const override { return m_unavailableReason; }

    void recognize(const QImage& image) override {
        cancel();
        if (!m_available) {
            emit recognitionFailed(m_unavailableReason.isEmpty() ? QStringLiteral("OCR is not available")
                                                                 : m_unavailableReason);
            return;
        }
        if (image.isNull()) {
            emit recognitionFailed(QStringLiteral("No screenshot image was provided"));
            return;
        }

        // Deep copy on the caller's thread: the QML-owned image may be reused,
        // and Format_RGB32 keeps bytesPerLine == width * 4 (tight rows), which
        // is what ncnn's from_pixels_resize expects.
        const QImage prepared = image.convertToFormat(QImage::Format_RGB32);
        const quint64 generation = ++m_generation;

        auto* watcher = new QFutureWatcher<OcrRunResult>(this);
        connect(watcher, &QFutureWatcher<OcrRunResult>::finished, this, [this, watcher, generation]() {
            const OcrRunResult result = watcher->result();
            watcher->deleteLater();
            if (generation != m_generation) {
                return;  // superseded by a newer recognize()/cancel()
            }
            if (!result.loadError.isEmpty()) {
                m_available = false;
                m_unavailableReason = result.loadError;
                emit availableChanged();
            }
            if (!result.error.isEmpty()) {
                emit recognitionFailed(result.error);
                return;
            }
            emit recognized(result.text);
        });
        m_future = QtConcurrent::run([this, prepared]() { return runPipeline(prepared); });
        watcher->setFuture(m_future);
    }

    void cancel() override {
        // A running inference cannot be interrupted cheaply; bump the
        // generation so its completion is dropped instead.
        ++m_generation;
    }

private:
    bool ensureNetsLoaded() {
        if (m_nets) {
            return true;
        }
        m_nets = std::make_unique<Nets>();
        // Options must be set before loading; newer ncnn reads the thread count
        // from opt (Extractor::set_num_threads is gone).
        m_nets->det.opt.use_vulkan_compute = false;
        m_nets->det.opt.lightmode = true;
        m_nets->det.opt.num_threads = m_numThreads;
        m_nets->rec.opt.use_vulkan_compute = false;
        m_nets->rec.opt.lightmode = true;
        m_nets->rec.opt.num_threads = m_numThreads;

        auto load = [](ncnn::Net& net, const QString& param, const QString& bin) {
            int status = net.load_param(QFile::encodeName(param).constData());
            status |= net.load_model(QFile::encodeName(bin).constData());
            return status;
        };

        const int detStatus = load(m_nets->det, m_paths.detParam, m_paths.detBin);
        const int recStatus = load(m_nets->rec, m_paths.recParam, m_paths.recBin);

        QFile keysFile(m_paths.keys);
        QStringList keys;
        if (keysFile.open(QIODevice::ReadOnly)) {
            keys = OcrPostProcess::parseDictionary(keysFile.readAll());
        }

        if (detStatus != 0 || recStatus != 0 || keys.isEmpty()) {
            m_nets.reset();
            m_loadError = QStringLiteral("OCR is not available: the bundled OCR models could not be loaded");
            qWarning() << m_loadError << "- models in" << m_paths.directory() << "det:" << detStatus
                       << "rec:" << recStatus << "keys:" << keys.size();
            return false;
        }

        m_keys = keys;
        qInfo() << "bundled OCR ready:" << keys.size() << "classes, models in" << m_paths.directory()
                << "threads:" << m_numThreads;
        return true;
    }

    // Runs on a worker thread.
    OcrRunResult runPipeline(const QImage& image) {
        OcrRunResult result;
        QMutexLocker guard(&m_mutex);
        if (!ensureNetsLoaded()) {
            result.loadError = m_loadError;
            result.error = m_loadError;
            return result;
        }

        const QImage source = image.convertToFormat(QImage::Format_RGB32);
        const int sourceWidth = source.width();
        const int sourceHeight = source.height();

        // --- detection -------------------------------------------------
        int detWidth = sourceWidth;
        int detHeight = sourceHeight;
        const int longSide = std::max(sourceWidth, sourceHeight);
        if (longSide > kDetTargetSize) {
            const double ratio = double(kDetTargetSize) / double(longSide);
            detWidth = int(double(sourceWidth) * ratio);
            detHeight = int(double(sourceHeight) * ratio);
        }
        detWidth = std::max((detWidth / kDetSizeAlignment) * kDetSizeAlignment, kDetSizeAlignment);
        detHeight = std::max((detHeight / kDetSizeAlignment) * kDetSizeAlignment, kDetSizeAlignment);
        const float scaleX = float(detWidth) / float(sourceWidth);
        const float scaleY = float(detHeight) / float(sourceHeight);

        ncnn::Mat detInput =
            ncnn::Mat::from_pixels_resize(source.constBits(), ncnn::Mat::PIXEL_BGRA2BGR, sourceWidth, sourceHeight,
                                          int(source.bytesPerLine()), detWidth, detHeight);
        if (detInput.empty()) {
            result.error = QStringLiteral("OCR failed to prepare the screenshot");
            return result;
        }
        // PP-OCR detection normalisation (PaddleOCR defaults, ImageNet stats).
        const float detMean[3] = {0.485f * 255.f, 0.456f * 255.f, 0.406f * 255.f};
        const float detNorm[3] = {1.f / 0.229f / 255.f, 1.f / 0.224f / 255.f, 1.f / 0.225f / 255.f};
        detInput.substract_mean_normalize(detMean, detNorm);

        ncnn::Mat scoreMap;
        {
            ncnn::Extractor extractor = m_nets->det.create_extractor();
            extractor.input("input", detInput);
            if (extractor.extract("output", scoreMap) != 0 || scoreMap.empty()) {
                result.error = QStringLiteral("OCR detection failed");
                return result;
            }
        }

        std::vector<OcrPostProcess::OcrTextBox> boxes =
            OcrPostProcess::boxesFromProbabilityMap(static_cast<const float*>(scoreMap.data), scoreMap.w, scoreMap.h);
        OcrPostProcess::scaleBoxes(boxes, scaleX, scaleY, sourceWidth, sourceHeight);

        if (boxes.empty()) {
            result.text = QString();
            return result;
        }

        // --- recognition -----------------------------------------------
        // Recognition runs once per *visual row*, not per detected box. The
        // detector splits one line of text at wide gaps (a settings row's
        // "Language" and "English" are two boxes, so are "[Cancel]" and
        // "[Save]"); emitting one output line per box turned a 9-row dialog into
        // 17 lines and sent that wall of newlines to the translator.
        auto recognizeCrop = [this](const QImage& crop) {
            int recWidth = int(std::lround(double(kRecHeight) * double(crop.width()) / double(crop.height())));
            recWidth = std::clamp(recWidth, 1, kMaxRecWidth);

            ncnn::Mat recInput =
                ncnn::Mat::from_pixels_resize(crop.constBits(), ncnn::Mat::PIXEL_BGRA2BGR, crop.width(), crop.height(),
                                              int(crop.bytesPerLine()), recWidth, kRecHeight);
            if (recInput.empty()) {
                return QString();
            }
            // PP-OCR recognition normalisation: (x/255 - 0.5) / 0.5.
            const float recMean[3] = {127.5f, 127.5f, 127.5f};
            const float recNorm[3] = {1.f / 127.5f, 1.f / 127.5f, 1.f / 127.5f};
            recInput.substract_mean_normalize(recMean, recNorm);

            ncnn::Mat logits;
            ncnn::Extractor extractor = m_nets->rec.create_extractor();
            extractor.input("input", recInput);
            if (extractor.extract("output", logits) != 0 || logits.empty()) {
                return QString();
            }

            const OcrPostProcess::CtcResult decoded =
                OcrPostProcess::decodeCtc(static_cast<const float*>(logits.data), logits.h, logits.w, m_keys);
            if (decoded.text.isEmpty() || decoded.score < kMinLineScore) {
                return QString();
            }
            return decoded.text;
        };

        QStringList lines;
        for (const std::vector<OcrPostProcess::OcrTextBox>& row : OcrPostProcess::groupRows(boxes)) {
            QStringList parts;
            for (const OcrPostProcess::OcrTextBox& box : OcrPostProcess::mergeRowMembers(row)) {
                if (box.height < kMinCropHeight) {
                    continue;
                }
                const QImage crop = source.copy(box.x, box.y, box.width, box.height);
                if (crop.isNull() || crop.height() <= 0) {
                    continue;
                }
                const QString text = recognizeCrop(crop);
                if (!text.isEmpty()) {
                    parts << text;
                }
            }
            const QString line = OcrPostProcess::normalizeSpacing(OcrPostProcess::joinRowTexts(parts));
            if (!line.trimmed().isEmpty()) {
                lines << line;
            }
        }

        result.text = OcrPostProcess::joinLines(lines);
        return result;
    }

    OcrModelLocator::OcrModelPaths m_paths;
    QString m_unavailableReason;
    bool m_available = false;
    // ncnn parallelises inside one inference (OpenMP); this bounds it so OCR on
    // a worker thread cannot starve the UI of every core.
    int m_numThreads = 1;
    QStringList m_keys;
    std::unique_ptr<Nets> m_nets;
    QString m_loadError;
    QMutex m_mutex;
    quint64 m_generation = 0;
    QFuture<OcrRunResult> m_future;
};

std::unique_ptr<OcrEngine> createRapidOcrEngine() {
    return std::make_unique<RapidOcrEngine>();
}

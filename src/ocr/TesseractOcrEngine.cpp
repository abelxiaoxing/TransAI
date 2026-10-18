// Tesseract CLI OCR backend (Linux + Windows). Linking the C API would add a
// hard dependency; spawning the CLI keeps tesseract optional: systems without
// it simply report OCR as unavailable.
//
// Protocol: PNG bytes on stdin (`tesseract stdin stdout`), UTF-8 text on
// stdout. Availability and the language list are probed once and cached.

#include "ocr/OcrEngine.h"

#include <QBuffer>
#include <QDebug>
#include <QProcess>
#include <QStandardPaths>

namespace OcrLanguageSelection {

QStringList parseListLangsOutput(const QString& output)
{
    QStringList langs;
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        // Skip the header ("List of available languages in … (N):") and the
        // "osd" orientation-and-script pseudo language.
        if (line.isEmpty() || line.contains(QLatin1String("available languages"))
            || line == QLatin1String("osd")) {
            continue;
        }
        langs << line;
    }
    return langs;
}

QString chooseLanguages(const QStringList& available)
{
    const bool hasEng = available.contains(QLatin1String("eng"));
    const bool hasChiSim = available.contains(QLatin1String("chi_sim"));
    if (hasEng && hasChiSim) {
        return QStringLiteral("chi_sim+eng");
    }
    if (hasEng) {
        return QStringLiteral("eng");
    }
    if (!available.isEmpty()) {
        return available.first();
    }
    return QString();
}

} // namespace OcrLanguageSelection

namespace {

// Bounded probe budgets. Both run off the critical path (see startAsyncProbe),
// so they only matter for the synchronous fallback taken when the user starts
// a screenshot OCR job before the event loop got to the async probe.
constexpr int kVersionProbeTimeoutMs = 1500;
constexpr int kLanguageProbeTimeoutMs = 2000;

class TesseractOcrEngine : public OcrEngine
{
public:
    explicit TesseractOcrEngine(QObject* parent = nullptr)
        : OcrEngine(parent)
    {
        // Probing `tesseract --version` (and then --list-langs) used to run
        // synchronously in the constructor, on the thread that builds the QML
        // engine — up to 3.5s of dead startup on a cold cache. Launch it
        // asynchronously instead and publish the result through availableChanged.
        startAsyncProbe();
    }

    bool isAvailable() override { return m_available; }

    // The tesseract backend is now only a fallback for builds without the
    // bundled models, so the reason has to name the missing binary.
    QString unavailableReason() const override
    {
        return m_available
            ? QString()
            : QStringLiteral("OCR is not available on this system: the tesseract command was not found");
    }

    void recognize(const QImage& image) override
    {
        cancel();
        if (!m_probeDone) {
            // The user acted before the asynchronous probe finished.
            probeSynchronously();
        }
        if (!m_available) {
            emit recognitionFailed(QStringLiteral(
                "OCR is not available on this system: the tesseract command was not found"));
            return;
        }
        if (image.isNull()) {
            emit recognitionFailed(QStringLiteral("No screenshot image was provided"));
            return;
        }

        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG")) {
            emit recognitionFailed(QStringLiteral("Failed to encode the image for OCR"));
            return;
        }

        m_process = new QProcess(this);
        connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
            QProcess* process = m_process;
            m_process = nullptr;
            const QByteArray out = process->readAllStandardOutput();
            const QByteArray err = process->readAllStandardError();
            process->deleteLater();
            if (exitCode != 0) {
                const QString detail = QString::fromUtf8(err).split(QLatin1Char('\n')).first().trimmed();
                emit recognitionFailed(detail.isEmpty()
                    ? QStringLiteral("OCR failed")
                    : QStringLiteral("OCR failed: %1").arg(detail));
                return;
            }
            emit recognized(QString::fromUtf8(out));
        });

        QStringList args{ QStringLiteral("stdin"), QStringLiteral("stdout") };
        if (!m_languages.isEmpty()) {
            args << QStringLiteral("-l") << m_languages;
        }
        args << QStringLiteral("--psm") << QStringLiteral("6");

        m_process->start(m_program, args);
        if (!m_process->waitForStarted(1500)) {
            m_process->deleteLater();
            m_process = nullptr;
            emit recognitionFailed(QStringLiteral("OCR failed to start"));
            return;
        }
        m_process->write(png);
        m_process->closeWriteChannel();
    }

    void cancel() override
    {
        if (!m_process) {
            return;
        }
        QProcess* process = m_process;
        m_process = nullptr;
        disconnect(process, nullptr, this, nullptr);
        process->kill();
        process->deleteLater();
    }

private:
    // Single funnel for every probe path: caches the result and lets QML
    // re-evaluate its availability hints exactly once per outcome. The first
    // result wins — the asynchronous probe may still be in flight when the
    // synchronous fallback in recognize() completes.
    void setProbeResult(bool available, const QString& languages)
    {
        if (m_probeDone) {
            return;
        }
        m_available = available;
        m_languages = languages;
        m_probeDone = true;
        if (available && !languages.isEmpty()) {
            qInfo() << "OCR languages:" << languages;
        }
        // Emitted exactly once, and always through the event loop: on Windows a
        // missing binary fails synchronously inside the constructor, and a
        // signal emitted there reaches nobody (observers connect afterwards).
        // The property itself is already correct, so QML bindings see it either
        // way; this only guarantees the change notification arrives.
        QMetaObject::invokeMethod(
            this, [this]() { emit availableChanged(); }, Qt::QueuedConnection);
    }

    void startAsyncProbe()
    {
        // Resolve the binary ourselves: QProcess' PATH search on Windows made a
        // *missing* tesseract take tens of seconds before reporting
        // FailedToStart.
        m_program = QStandardPaths::findExecutable(QStringLiteral("tesseract"));
        if (m_program.isEmpty()) {
            setProbeResult(false, QString());
            return;
        }

        auto* versionProbe = new QProcess(this);
        connect(versionProbe, &QProcess::errorOccurred, this,
                [this, versionProbe](QProcess::ProcessError error) {
                    if (error != QProcess::FailedToStart) {
                        return;
                    }
                    // Absent binary: no finished() follows a failed start.
                    versionProbe->deleteLater();
                    setProbeResult(false, QString());
                });
        connect(versionProbe, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this, versionProbe](int exitCode, QProcess::ExitStatus) {
                    versionProbe->deleteLater();
                    if (m_probeDone) {
                        return; // the synchronous fallback already answered
                    }
                    if (exitCode != 0) {
                        setProbeResult(false, QString());
                        return;
                    }
                    startAsyncLanguageProbe();
                });
        versionProbe->start(m_program, { QStringLiteral("--version") });
    }

    void startAsyncLanguageProbe()
    {
        auto* langsProbe = new QProcess(this);
        connect(langsProbe, &QProcess::errorOccurred, this,
                [this, langsProbe](QProcess::ProcessError error) {
                    if (error != QProcess::FailedToStart) {
                        return;
                    }
                    langsProbe->deleteLater();
                    setProbeResult(true, QString()); // available, default languages
                });
        connect(langsProbe, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this, langsProbe](int exitCode, QProcess::ExitStatus) {
                    const QByteArray out = langsProbe->readAllStandardOutput();
                    langsProbe->deleteLater();
                    // A failing --list-langs leaves the language set empty,
                    // which means "tesseract default".
                    setProbeResult(true, exitCode == 0
                        ? OcrLanguageSelection::chooseLanguages(
                              OcrLanguageSelection::parseListLangsOutput(QString::fromUtf8(out)))
                        : QString());
                });
        langsProbe->start(m_program, { QStringLiteral("--list-langs") });
    }

    // Only used when recognize() arrives before the async probe completed.
    void probeSynchronously()
    {
        if (m_probeDone) {
            return;
        }
        if (m_program.isEmpty()) {
            m_program = QStandardPaths::findExecutable(QStringLiteral("tesseract"));
        }
        if (m_program.isEmpty()) {
            setProbeResult(false, QString());
            return;
        }

        QProcess version;
        version.start(m_program, { QStringLiteral("--version") });
        if (!version.waitForFinished(kVersionProbeTimeoutMs) || version.exitCode() != 0) {
            setProbeResult(false, QString());
            return;
        }

        QProcess langs;
        langs.start(m_program, { QStringLiteral("--list-langs") });
        QString languages;
        if (langs.waitForFinished(kLanguageProbeTimeoutMs) && langs.exitCode() == 0) {
            languages = OcrLanguageSelection::chooseLanguages(
                OcrLanguageSelection::parseListLangsOutput(QString::fromUtf8(langs.readAllStandardOutput())));
        }
        setProbeResult(true, languages);
    }

    bool m_available = false;
    bool m_probeDone = false;
    QString m_program;
    QString m_languages;
    QProcess* m_process = nullptr;
};

} // namespace

std::unique_ptr<OcrEngine> createTesseractOcrEngine()
{
    return std::make_unique<TesseractOcrEngine>();
}

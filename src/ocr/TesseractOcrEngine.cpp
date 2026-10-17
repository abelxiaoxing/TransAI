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

class TesseractOcrEngine : public OcrEngine
{
public:
    explicit TesseractOcrEngine(QObject* parent = nullptr)
        : OcrEngine(parent)
    {
        // `tesseract --version` exits instantly when present and fails fast
        // when absent, so a bounded synchronous probe at startup is fine.
        QProcess probe;
        probe.start(QStringLiteral("tesseract"), { QStringLiteral("--version") });
        m_available = probe.waitForFinished(1500) && probe.exitCode() == 0;
        if (m_available) {
            probeLanguages();
        }
    }

    bool isAvailable() override { return m_available; }

    void recognize(const QImage& image) override
    {
        cancel();
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

        m_process->start(QStringLiteral("tesseract"), args);
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
    void probeLanguages()
    {
        QProcess probe;
        probe.start(QStringLiteral("tesseract"), { QStringLiteral("--list-langs") });
        if (!probe.waitForFinished(2000) || probe.exitCode() != 0) {
            return; // empty list → tesseract default languages
        }
        m_languages = OcrLanguageSelection::chooseLanguages(
            OcrLanguageSelection::parseListLangsOutput(QString::fromUtf8(probe.readAllStandardOutput())));
        qInfo() << "OCR languages:" << m_languages;
    }

    bool m_available = false;
    QString m_languages;
    QProcess* m_process = nullptr;
};

} // namespace

std::unique_ptr<OcrEngine> createOcrEngine()
{
    return std::make_unique<TesseractOcrEngine>();
}

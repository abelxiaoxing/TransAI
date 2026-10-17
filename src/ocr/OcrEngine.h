#ifndef OCR_ENGINE_H
#define OCR_ENGINE_H

#include <memory>

#include <QImage>
#include <QObject>

// Platform OCR engine for the screenshot-translation OCR path
// (screenshotTranslateMode "ocr", or "auto" without a vision-capable model).
//
// One backend is compiled per platform (exactly one source provides
// createOcrEngine):
//   Linux / Windows — TesseractOcrEngine: runs the `tesseract` CLI through
//                     QProcess (no library linkage; absent binary →
//                     available=false and the UI explains how to install it);
//   macOS           — VisionOcrEngine: VNRecognizeTextRequest (system API);
//   otherwise       — nullptr, OCR mode reports itself unavailable.
//
// Recognition is asynchronous: recognize() returns immediately and exactly
// one of recognized()/recognitionFailed() follows (a new recognize() cancels
// the previous job first).
class OcrEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable CONSTANT)

public:
    using QObject::QObject;

    virtual bool isAvailable() = 0;
    virtual void recognize(const QImage& image) = 0;
    Q_INVOKABLE virtual void cancel() = 0;

signals:
    void recognized(const QString& text);
    void recognitionFailed(const QString& message);
};

// Returns nullptr when no backend is compiled for this platform.
std::unique_ptr<OcrEngine> createOcrEngine();

// Pure helpers (unit-tested): parse `tesseract --list-langs` output and pick
// the recognition language set.
namespace OcrLanguageSelection {
// Parses `tesseract --list-langs` stdout into the available language codes.
QStringList parseListLangsOutput(const QString& output);
// Preferred set: "chi_sim+eng" when both packs exist, then "eng", then the
// first available pack, else empty (tesseract built-in default).
QString chooseLanguages(const QStringList& available);
} // namespace OcrLanguageSelection

#endif // OCR_ENGINE_H

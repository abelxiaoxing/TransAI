#ifndef OCR_ENGINE_H
#define OCR_ENGINE_H

#include <memory>

#include <QImage>
#include <QObject>
#include <QString>

// Platform OCR engine for the screenshot-translation OCR path
// (screenshotTranslateMode "ocr", or "auto" without a vision-capable model).
//
// Backends (OcrEngineFactory.cpp picks exactly one at compile time):
//   Linux / Windows — RapidOcrEngine: bundled PP-OCR det+rec models running on
//                     ncnn, no external process and no system installation;
//                     when the bundled backend is disabled or its models are
//                     missing the tesseract CLI backend is the fallback;
//   macOS           — VisionOcrEngine: VNRecognizeTextRequest (system API);
//   otherwise       — unavailable backend, reporting why.
//
// Recognition is asynchronous: recognize() returns immediately and exactly
// one of recognized()/recognitionFailed() follows (a new recognize() cancels
// the previous job first).
//
// Availability is reported through a NOTIFY property because a backend may
// probe or fail to load its models off the startup path; QML rebinds the
// routing hints when it flips, and `unavailableReason` explains what to fix.
class OcrEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(QString unavailableReason READ unavailableReason NOTIFY availableChanged)

public:
    using QObject::QObject;

    virtual bool isAvailable() = 0;
    // Human-readable explanation of why isAvailable() is false; empty when the
    // engine is available. The English text doubles as an I18n table key where
    // the message is fixed.
    virtual QString unavailableReason() const { return QString(); }
    // Q_INVOKABLE because QML is the only caller (AppView's screenshot OCR
    // leg). Without it QML can see the method but cannot call it: the call
    // silently becomes "TypeError: Property 'recognize' ... is not a function",
    // no signal ever arrives, and the status pill sits on "Recognizing text..."
    // forever. tests/tst_rapidocr.cpp::recognizeIsCallableFromQml guards it.
    Q_INVOKABLE virtual void recognize(const QImage& image) = 0;
    Q_INVOKABLE virtual void cancel() = 0;

signals:
    void availableChanged();
    void recognized(const QString& text);
    void recognitionFailed(const QString& message);
};

// Single entry point used by main.cpp: returns the backend compiled for this
// platform (never nullptr).
std::unique_ptr<OcrEngine> createOcrEngine();

// Backend factories, each defined in its own translation unit so the tests can
// link a single backend. Not all of them exist in every build.
std::unique_ptr<OcrEngine> createRapidOcrEngine();      // TRANSAI_OCR_BACKEND_NCNN
std::unique_ptr<OcrEngine> createTesseractOcrEngine();  // TRANSAI_OCR_BACKEND_TESSERACT
std::unique_ptr<OcrEngine> createVisionOcrEngine();     // TRANSAI_OCR_BACKEND_VISION
std::unique_ptr<OcrEngine> createUnavailableOcrEngine();

// Pure helpers (unit-tested): parse `tesseract --list-langs` output and pick
// the recognition language set.
namespace OcrLanguageSelection {
// Parses `tesseract --list-langs` stdout into the available language codes.
QStringList parseListLangsOutput(const QString& output);
// Preferred set: "chi_sim+eng" when both packs exist, then "eng", then the
// first available pack, else empty (tesseract built-in default).
QString chooseLanguages(const QStringList& available);
}  // namespace OcrLanguageSelection

#endif  // OCR_ENGINE_H

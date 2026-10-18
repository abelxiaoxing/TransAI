// Single place that decides which OCR backend the application uses. Each
// backend lives in its own translation unit and exposes its own factory, so the
// tests can link exactly one of them (and so the platform CMake blocks can pick
// a subset without duplicating the entry point).

#include "ocr/OcrEngine.h"

std::unique_ptr<OcrEngine> createOcrEngine() {
#if defined(TRANSAI_OCR_BACKEND_VISION)
    // macOS: VNRecognizeTextRequest — system-provided, nothing to bundle.
    return createVisionOcrEngine();
#elif defined(TRANSAI_OCR_BACKEND_NCNN)
    // Bundled PP-OCR/ncnn backend: self-contained, no user installation.
    return createRapidOcrEngine();
#elif defined(TRANSAI_OCR_BACKEND_TESSERACT)
    // Fallback when the bundled backend is disabled at build time.
    return createTesseractOcrEngine();
#else
    // macOS uses the Vision framework; other platforms without any backend.
    return createUnavailableOcrEngine();
#endif
}

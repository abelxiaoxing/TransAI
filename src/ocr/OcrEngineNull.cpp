// Fallback backend for platforms without a real OCR engine: reports itself
// unavailable so the QML side always has a valid "ocrEngine" object to bind
// (no null context-property guards needed anywhere).

#include "ocr/OcrEngine.h"

namespace {

class UnavailableOcrEngine : public OcrEngine {
public:
    using OcrEngine::OcrEngine;

    bool isAvailable() override { return false; }

    QString unavailableReason() const override { return QStringLiteral("OCR is not available on this platform"); }

    void recognize(const QImage& image) override {
        Q_UNUSED(image);
        emit recognitionFailed(unavailableReason());
    }

    void cancel() override {}
};

}  // namespace

std::unique_ptr<OcrEngine> createUnavailableOcrEngine() {
    return std::make_unique<UnavailableOcrEngine>();
}

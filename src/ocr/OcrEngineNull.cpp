// Fallback backend for platforms without a real OCR engine: reports itself
// unavailable so the QML side always has a valid "ocrEngine" object to bind
// (no null context-property guards needed anywhere).

#include "ocr/OcrEngine.h"

namespace {

class UnavailableOcrEngine : public OcrEngine
{
public:
    using OcrEngine::OcrEngine;

    bool isAvailable() override { return false; }

    void recognize(const QImage& image) override
    {
        Q_UNUSED(image);
        emit recognitionFailed(QStringLiteral("OCR is not available on this platform"));
    }

    void cancel() override {}
};

} // namespace

std::unique_ptr<OcrEngine> createOcrEngine()
{
    return std::make_unique<UnavailableOcrEngine>();
}

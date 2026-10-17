#include "vision/ImagePayloadBuilder.h"

#include <QBuffer>
#include <QPainter>

namespace {
constexpr int kDefaultMaxWidth = 1600;
constexpr int kDefaultJpegQuality = 85;
constexpr int kMinDimension = 8;
} // namespace

ImagePayloadResult ImagePayloadBuilder::buildDataUrl(const QImage& image, int maxWidth, int jpegQuality)
{
    ImagePayloadResult result;

    if (image.isNull()) {
        result.error = QStringLiteral("The captured image is empty");
        return result;
    }
    if (image.width() < kMinDimension || image.height() < kMinDimension) {
        result.error = QStringLiteral("The captured image is too small");
        return result;
    }

    if (maxWidth <= 0) {
        maxWidth = kDefaultMaxWidth;
    }
    if (jpegQuality < 0 || jpegQuality > 100) {
        jpegQuality = kDefaultJpegQuality;
    }

    QImage prepared = image;
    if (prepared.width() > maxWidth) {
        const int scaledHeight = qMax(1, prepared.height() * maxWidth / prepared.width());
        prepared = prepared.scaled(maxWidth, scaledHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    // JPEG has no alpha channel; composite onto white first so transparent
    // regions (e.g. screenshots with rounded corners) don't turn black.
    if (prepared.hasAlphaChannel()) {
        QImage flattened(prepared.size(), QImage::Format_RGB32);
        flattened.fill(Qt::white);
        QPainter painter(&flattened);
        painter.drawImage(0, 0, prepared);
        painter.end();
        prepared = flattened;
    }

    QByteArray jpegData;
    {
        QBuffer buffer(&jpegData);
        buffer.open(QIODevice::WriteOnly);
        if (!prepared.save(&buffer, "JPEG", jpegQuality)) {
            result.error = QStringLiteral("Failed to encode the image as JPEG");
            return result;
        }
    }
    if (jpegData.isEmpty()) {
        result.error = QStringLiteral("Failed to encode the image as JPEG");
        return result;
    }

    result.ok = true;
    result.finalSize = prepared.size();
    result.dataUrl = QByteArrayLiteral("data:image/jpeg;base64,") + jpegData.toBase64();
    return result;
}

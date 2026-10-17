#ifndef IMAGE_PAYLOAD_BUILDER_H
#define IMAGE_PAYLOAD_BUILDER_H

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>

struct ImagePayloadResult {
    bool ok = false;
    // Human-readable reason when ok == false.
    QString error;
    // "data:image/jpeg;base64,..." ready for an OpenAI-compatible
    // image_url payload.
    QByteArray dataUrl;
    // Image size after scaling (== input size when no scaling was needed).
    QSize finalSize;
};

// Pure image-processing helper: QImage -> scaled JPEG -> base64 data URL.
// No network access. QML never touches base64.
class ImagePayloadBuilder
{
public:
    // maxWidth <= 0 means "no scaling"; jpegQuality follows QImageWriter
    // semantics (-1 = default, 0-100 = quality). Out-of-range values are
    // clamped to sane defaults.
    static ImagePayloadResult buildDataUrl(const QImage& image, int maxWidth, int jpegQuality);
};

#endif // IMAGE_PAYLOAD_BUILDER_H

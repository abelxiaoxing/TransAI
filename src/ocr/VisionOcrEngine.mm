// macOS backend: Vision framework text recognition (VNRecognizeTextRequest).
// Compiled only on Darwin (see CMakeLists.txt).

#include "ocr/OcrEngine.h"

#include <QCoreApplication>
#include <QDebug>
#include <QPointer>

#import <Vision/Vision.h>

namespace {

// QImage → CGImage without pulling in private Qt headers (defined below).
CGImageRef CGImageCreateFromQImage(const QImage& image);

class VisionOcrEngine : public OcrEngine
{
public:
    using OcrEngine::OcrEngine;

    // VNRecognizeTextRequest exists since macOS 10.15; supported targets.
    bool isAvailable() override { return true; }

    void recognize(const QImage& image) override
    {
        cancel();
        if (image.isNull()) {
            emit recognitionFailed(QStringLiteral("No screenshot image was provided"));
            return;
        }

        // Keep a deep copy: the QImage passed from QML may be reused.
        const QImage copy = image.convertToFormat(QImage::Format_RGBA8888);
        CGImageRef cgImage = CGImageCreateFromQImage(copy); // see below
        if (!cgImage) {
            emit recognitionFailed(QStringLiteral("Failed to encode the image for OCR"));
            return;
        }

        m_running = true;
        // Vision is CPU/GPU-bound and synchronous; move it off the UI thread.
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
            VNImageRequestHandler* handler =
                [[VNImageRequestHandler alloc] initWithCGImage:cgImage options:@{}];
            VNRecognizeTextRequest* request = [[VNRecognizeTextRequest alloc] init];
            request.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
            request.recognitionLanguages = @[ @"zh-Hans", @"en" ];
            request.usesLanguageCorrection = YES;

            NSError* error = nil;
            [handler performRequests:@[ request ] error:&error];
            CGImageRelease(cgImage);

            QString text;
            QString failure;
            if (error) {
                failure = QStringLiteral("OCR failed: %1")
                    .arg(QString::fromNSString(error.localizedDescription));
            } else {
                QStringList lines;
                for (VNRecognizedTextObservation* observation in request.results) {
                    VNRecognizedText* candidate = [observation topCandidates:1].firstObject;
                    if (candidate) {
                        lines << QString::fromNSString(candidate.string);
                    }
                }
                text = lines.join(QLatin1Char('\n'));
            }

            // Back to the Qt thread; dropped when a newer recognize() or
            // cancel() superseded this job.
            QPointer<VisionOcrEngine> self(this);
            QMetaObject::invokeMethod(qApp, [self, text, failure]() {
                if (!self || !self->m_running) {
                    return;
                }
                self->m_running = false;
                if (!failure.isEmpty()) {
                    emit self->recognitionFailed(failure);
                } else {
                    emit self->recognized(text);
                }
            }, Qt::QueuedConnection);
        });
    }

    void cancel() override
    {
        // The Vision request itself cannot be cancelled cheaply; mark the
        // job superseded so its completion is dropped.
        m_running = false;
    }

private:
    bool m_running = false;
};

CGImageRef CGImageCreateFromQImage(const QImage& image)
{
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    const int width = rgba.width();
    const int height = rgba.height();
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        const_cast<uchar*>(rgba.constBits()), width, height, 8, rgba.bytesPerLine(),
        colorSpace, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorSpace);
    if (!context) {
        return nullptr;
    }
    CGImageRef cgImage = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    return cgImage; // bitmap data is copied into the image
}

} // namespace

std::unique_ptr<OcrEngine> createOcrEngine()
{
    return std::make_unique<VisionOcrEngine>();
}

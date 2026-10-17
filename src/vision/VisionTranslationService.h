#ifndef VISION_TRANSLATION_SERVICE_H
#define VISION_TRANSLATION_SERVICE_H

#include <QImage>

#include "translation/StreamingChatClient.h"

// Streaming vision-chat translation: image -> multimodal LLM -> translated
// text. All networking, SSE parsing, timeouts and the thinking-params retry
// live in StreamingChatClient; this subclass only validates the request,
// encodes the image (ImagePayloadBuilder) and assembles the multimodal
// chat-completion body:
//   messages = [ system(text), user(content:[text, image_url(data-url)]) ]
// It also rewrites raw "image not supported" API errors into user guidance
// (decorateFailureMessage).
class VisionTranslationService : public StreamingChatClient
{
    Q_OBJECT

public:
    explicit VisionTranslationService(QObject* parent = nullptr);

    // Explicit-parameter request API: arity/type mistakes surface at the
    // call site instead of silently missing QVariantMap keys.
    Q_INVOKABLE void translateImage(const QImage& image, const QString& targetLanguage,
                                    const QString& provider, const QString& apiServer,
                                    const QString& apiKey, const QString& model,
                                    int maxImageWidth, int jpegQuality);

protected:
    QString decorateFailureMessage(const QString& error) const override;

private:
    struct VisionRequest {
        QString targetLanguage;
        QString provider;
        QString apiServer;
        QString apiKey;
        QString model;
        QByteArray imageDataUrl;
    };

    PendingRequest buildPendingRequest(const VisionRequest& request) const;
    // True when an API error message indicates the endpoint/model rejected
    // image input; the failure text is then rewritten with guidance.
    static bool isVisionUnsupportedError(const QString& errorMessage);
};

#endif // VISION_TRANSLATION_SERVICE_H

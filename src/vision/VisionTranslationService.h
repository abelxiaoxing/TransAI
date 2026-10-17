#ifndef VISION_TRANSLATION_SERVICE_H
#define VISION_TRANSLATION_SERVICE_H

#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QVariantMap>

#include "translation/StreamParser.h"

// Streaming vision-chat translation: image -> multimodal LLM -> translated
// text. Independent from TranslationService (text-only); the two never hold
// each other. The signal shape mirrors TranslationService so the QML side
// can bind both services identically.
//
// Request payload is OpenAI-compatible vision chat:
//   messages = [ system(text), user(content:[text, image_url(data-url)]) ]
// URL normalization (ProviderEndpointBuilder), thinking suppression
// (ThinkingPolicyBuilder, including the transparent retry on rejection) and
// SSE parsing (StreamParser) are shared with the text path.
class VisionTranslationService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString responseData READ responseData NOTIFY responseDataChanged)
    Q_PROPERTY(QString responseError READ responseError NOTIFY responseErrorChanged)
    Q_PROPERTY(bool isRequesting READ isRequesting NOTIFY isRequestingChanged)

public:
    explicit VisionTranslationService(QObject* parent = nullptr);

    QString responseData() const;
    QString responseError() const;
    bool isRequesting() const;

    // options keys: targetLanguage, provider, apiServer, apiKey, model,
    // maxImageWidth (int), jpegQuality (int).
    Q_INVOKABLE void translateImage(const QImage& image, const QVariantMap& options);
    Q_INVOKABLE void abort();

signals:
    void responseDataChanged();
    void responseErrorChanged();
    void isRequestingChanged();

    void translationStarted();
    void translationFinished(QString result);
    void translationFailed(QString error);
    void translationStopped();

private:
    struct VisionRequest {
        QString targetLanguage;
        QString provider;
        QString apiServer;
        QString apiKey;
        QString model;
        QByteArray imageDataUrl;
    };

    void startRequest(const VisionRequest& request);
    void sendChatRequest(const VisionRequest& request);
    void cancelCurrentRequest();
    void handleStreamData(QNetworkReply* reply);
    void setResponseData(const QString& responseData);
    void setResponseError(const QString& responseError);
    void setIsRequesting(bool isRequesting);
    QString networkErrorMessage(QNetworkReply::NetworkError error) const;
    // True when an API error message indicates the endpoint/model rejected
    // image input; the failure text is then rewritten with guidance.
    static bool isVisionUnsupportedError(const QString& errorMessage);

    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_reply;
    QString m_responseData;
    QString m_responseError;
    bool m_isRequesting;
    bool m_thinkingParamsAutoDisabled = false;
    StreamParser m_streamParser;
};

#endif // VISION_TRANSLATION_SERVICE_H

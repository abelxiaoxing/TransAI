#ifndef STREAMING_CHAT_CLIENT_H
#define STREAMING_CHAT_CLIENT_H

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>

#include "translation/StreamParser.h"

class QTimer;

// Shared streaming chat-completion client: owns the QNetworkAccessManager,
// the single in-flight reply, SSE parsing (StreamParser), the 30s inactivity
// timeout, an overall 5-minute cap, the transparent thinking-params retry,
// and the "exactly one terminal signal per request" model.
//
// TranslationService (text) and VisionTranslationService (multimodal) both
// derive from this; the base knows nothing about translation — subclasses
// only validate inputs and assemble a PendingRequest (URL + JSON body).
//
// The QML-facing shape (properties, signals, abort()) is identical to the
// former standalone services, so views bind both subclasses uniformly.
class StreamingChatClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString responseData READ responseData NOTIFY responseDataChanged)
    Q_PROPERTY(QString responseError READ responseError NOTIFY responseErrorChanged)
    Q_PROPERTY(bool isRequesting READ isRequesting NOTIFY isRequestingChanged)

public:
    explicit StreamingChatClient(QObject* parent = nullptr);

    QString responseData() const;
    QString responseError() const;
    bool isRequesting() const;

    // User-initiated stop: disconnects the reply before cancelling it, so the
    // finished handler never fires for it. Emits translationStopped.
    Q_INVOKABLE void abort();

signals:
    void responseDataChanged();
    void responseErrorChanged();
    void isRequestingChanged();

    // Exactly one of Finished/Failed/Stopped is emitted per request.
    void translationStarted();
    void translationFinished(QString result);
    void translationFailed(QString error);
    void translationStopped();

protected:
    // A fully assembled request. bodyWithoutThinking is the equivalent body
    // with the automatic thinking-suppression fields removed (equal to body
    // when the policy added none); only when the two differ may a rejected
    // request be transparently retried once without them.
    struct PendingRequest {
        QNetworkRequest networkRequest;
        QJsonObject body;
        QJsonObject bodyWithoutThinking;
    };

    // Entry point for subclasses after successful validation: cancels any
    // in-flight request, resets state, emits translationStarted and sends.
    void startRequest(const PendingRequest& request);

    // Validation-failure path: records the error and emits translationFailed
    // without ever emitting translationStarted (isRequesting stays false).
    void failValidation(const QString& message);

    // Last-chance rewrite of a failure message before it is recorded and
    // emitted (the vision subclass turns raw "image not supported" API
    // errors into user guidance). Default: identity.
    virtual QString decorateFailureMessage(const QString& error) const;

private:
    void sendPendingRequest();
    void cancelCurrentRequest();
    void handleStreamData(QNetworkReply* reply);
    void setResponseData(const QString& responseData);
    void setResponseError(const QString& responseError);
    void setIsRequesting(bool isRequesting);
    static QString networkErrorMessage(QNetworkReply::NetworkError error);

    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_reply = nullptr;
    // Single-shot cap on the whole request (the 30s transfer timeout only
    // covers inactivity; a slow-but-steady model would stream forever).
    QTimer* m_overallTimeout;
    PendingRequest m_pendingRequest;
    QString m_responseData;
    QString m_responseError;
    bool m_isRequesting = false;
    // Set when the endpoint rejected the thinking request fields and the
    // transparent retry (bodyWithoutThinking) was already spent.
    bool m_thinkingRetryUsed = false;
    StreamParser m_streamParser;
};

#endif // STREAMING_CHAT_CLIENT_H

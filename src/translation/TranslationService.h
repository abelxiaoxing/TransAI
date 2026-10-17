#ifndef TRANSLATION_SERVICE_H
#define TRANSLATION_SERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVariantMap>

#include "translation/StreamParser.h"
#include "translation/TranslationRequest.h"

class TranslationService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString responseData READ responseData NOTIFY responseDataChanged)
    Q_PROPERTY(QString responseError READ responseError NOTIFY responseErrorChanged)
    Q_PROPERTY(bool isRequesting READ isRequesting NOTIFY isRequestingChanged)

public:
    explicit TranslationService(QObject* parent = nullptr);

    QString responseData() const;
    QString responseError() const;
    bool isRequesting() const;

    Q_INVOKABLE void translateFromMap(const QVariantMap& requestMap);
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
    void translate(const TranslationRequest& request);
    void sendChatRequest(const TranslationRequest& request);
    void cancelCurrentRequest();
    void handleStreamData(QNetworkReply* reply);
    void setResponseData(const QString& responseData);
    void setResponseError(const QString& responseError);
    void setIsRequesting(bool isRequesting);
    QString networkErrorMessage(QNetworkReply::NetworkError error) const;

    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_reply;
    QString m_responseData;
    QString m_responseError;
    bool m_isRequesting;
    // Set when an endpoint rejected the automatic thinking request fields;
    // the transparent retry in sendChatRequest resends without them.
    bool m_thinkingParamsAutoDisabled = false;
    StreamParser m_streamParser;
};

#endif // TRANSLATION_SERVICE_H

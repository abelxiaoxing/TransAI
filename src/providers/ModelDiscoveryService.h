#ifndef MODEL_DISCOVERY_SERVICE_H
#define MODEL_DISCOVERY_SERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStringList>

class ModelDiscoveryService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList availableModels READ availableModels NOTIFY availableModelsChanged)
    Q_PROPERTY(bool isDetecting READ isDetecting NOTIFY isDetectingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit ModelDiscoveryService(QObject* parent = nullptr);

    QStringList availableModels() const;
    bool isDetecting() const;
    QString error() const;

    Q_INVOKABLE void detectModels(QString apiServer, QString apiKey, QString provider);

signals:
    void availableModelsChanged();
    void isDetectingChanged();
    void errorChanged();

private:
    void cancelCurrentRequest();
    void setAvailableModels(const QStringList& availableModels);
    void setIsDetecting(bool isDetecting);
    void setError(const QString& error);
    QStringList parseModelList(const QByteArray& data, const QString& provider, QString* errorMessage) const;

    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_reply;
    QStringList m_availableModels;
    bool m_isDetecting;
    QString m_error;
};

#endif // MODEL_DISCOVERY_SERVICE_H

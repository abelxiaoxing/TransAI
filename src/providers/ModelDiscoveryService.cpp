#include "providers/ModelDiscoveryService.h"

#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkRequest>

#include "providers/ApiError.h"
#include "providers/ProviderDefaults.h"
#include "providers/ProviderEndpointBuilder.h"

ModelDiscoveryService::ModelDiscoveryService(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_isDetecting(false)
{
    m_networkManager->setTransferTimeout(15 * 1000);
}

QStringList ModelDiscoveryService::availableModels() const
{
    return m_availableModels;
}

bool ModelDiscoveryService::isDetecting() const
{
    return m_isDetecting;
}

QString ModelDiscoveryService::error() const
{
    return m_error;
}

void ModelDiscoveryService::detectModels(QString apiServer, QString apiKey, QString provider)
{
    cancelCurrentRequest();

    const QString normalizedProvider = ProviderDefaults::normalizedProviderName(provider);
    setError("");
    setAvailableModels(QStringList());

    if (normalizedProvider != "ollama" && !ProviderDefaults::isPlausibleApiKey(apiKey)) {
        setIsDetecting(false);
        setError(ProviderDefaults::apiKeyErrorMessage());
        return;
    }

    setIsDetecting(true);

    QNetworkRequest request(ProviderEndpointBuilder::buildModelListUrl(apiServer, normalizedProvider));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    if (normalizedProvider != "ollama") {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey.trimmed()).toUtf8());
    }

    m_reply = m_networkManager->get(request);
    QNetworkReply* currentReply = m_reply;
    connect(currentReply, &QNetworkReply::finished, this, [this, currentReply, normalizedProvider]() {
        if (m_reply != currentReply) {
            currentReply->deleteLater();
            return;
        }
        m_reply = nullptr;
        setIsDetecting(false);

        const QByteArray response = currentReply->readAll();
        QString parseError;
        const QStringList models = parseModelList(response, normalizedProvider, &parseError);
        setAvailableModels(models);

        if (currentReply->error() == QNetworkReply::NoError) {
            setError(models.isEmpty() ? (parseError.isEmpty() ? "No models found" : parseError) : "");
        } else if (currentReply->error() == QNetworkReply::OperationCanceledError) {
            // A canceled reply cannot reach finished via cancelCurrentRequest()
            // (it disconnects the reply first), so this can only be the 15s
            // transfer timeout. Report it instead of failing silently.
            setError("Model detection timed out: no data from the server");
        } else {
            setError(parseError.isEmpty() ? currentReply->errorString() : parseError);
        }

        currentReply->deleteLater();
    });
}

void ModelDiscoveryService::cancelCurrentRequest()
{
    if (!m_reply) {
        return;
    }

    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
    setIsDetecting(false);
}

void ModelDiscoveryService::setAvailableModels(const QStringList& availableModels)
{
    if (m_availableModels == availableModels) {
        return;
    }
    m_availableModels = availableModels;
    emit availableModelsChanged();
}

void ModelDiscoveryService::setIsDetecting(bool isDetecting)
{
    if (m_isDetecting == isDetecting) {
        return;
    }
    m_isDetecting = isDetecting;
    emit isDetectingChanged();
}

void ModelDiscoveryService::setError(const QString& error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

QStringList ModelDiscoveryService::parseModelList(const QByteArray& data, const QString& provider, QString* errorMessage) const
{
    QStringList models;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = parseError.errorString();
        }
        return models;
    }

    const QJsonObject obj = doc.object();
    if (obj.contains("error")) {
        if (errorMessage) {
            *errorMessage = ApiError::messageFromValue(obj.value("error"), QStringLiteral("Failed to detect models"));
        }
        return models;
    }

    const QJsonArray modelArray = provider == "ollama" ? obj.value("models").toArray() : obj.value("data").toArray();
    for (const QJsonValue& value : modelArray) {
        const QJsonObject modelObj = value.toObject();
        const QString id = provider == "ollama" ? modelObj.value("name").toString() : modelObj.value("id").toString();
        if (!id.isEmpty() && !models.contains(id)) {
            models.append(id);
        }
    }

    std::sort(models.begin(), models.end(), [](const QString& a, const QString& b) {
        return QString::localeAwareCompare(a, b) < 0;
    });
    return models;
}

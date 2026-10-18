#include "providers/ModelDiscoveryService.h"

#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkRequest>
#include <QTimer>

#include "providers/ApiError.h"
#include "providers/NetworkErrorMapper.h"
#include "providers/ProviderDefaults.h"
#include "providers/ProviderEndpointBuilder.h"

namespace {
// Whole-request cap; detection is a single small GET, so 30s is generous.
constexpr int kOverallTimeoutMs = 30 * 1000;
} // namespace

ModelDiscoveryService::ModelDiscoveryService(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_overallTimeout(new QTimer(this))
    , m_isDetecting(false)
{
    m_networkManager->setTransferTimeout(15 * 1000);
    m_overallTimeout->setSingleShot(true);
    m_overallTimeout->setInterval(kOverallTimeoutMs);
    connect(m_overallTimeout, &QTimer::timeout, this, [this]() {
        if (!m_reply) {
            return;
        }
        // Record the reason, then abort WITHOUT disconnecting: the finished
        // handler keeps the recorded message and emits the terminal state.
        setError(QStringLiteral("Model detection timed out: the server took too long to respond"));
        m_reply->abort();
    });
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

    if (!ProviderDefaults::isOllamaProvider(normalizedProvider) && !ProviderDefaults::isPlausibleApiKey(apiKey)) {
        setIsDetecting(false);
        setError(ProviderDefaults::apiKeyErrorMessage());
        return;
    }

    setIsDetecting(true);

    QNetworkRequest request(ProviderEndpointBuilder::buildModelListUrl(apiServer, normalizedProvider));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    if (!ProviderDefaults::isOllamaProvider(normalizedProvider)) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey.trimmed()).toUtf8());
    }

    m_reply = m_networkManager->get(request);
    QNetworkReply* currentReply = m_reply;
    m_overallTimeout->start();
    connect(currentReply, &QNetworkReply::finished, this, [this, currentReply, normalizedProvider]() {
        if (m_reply != currentReply) {
            currentReply->deleteLater();
            return;
        }
        m_overallTimeout->stop();
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
            // transfer timeout or the 30s overall cap. Both record a message;
            // only fall back when none is present.
            if (m_error.isEmpty()) {
                setError(QStringLiteral("Model detection timed out: no data from the server"));
            }
        } else {
            // Prefer the server's own payload message; otherwise reuse the
            // shared, translatable network-error mapping (identical wording to
            // the translation pipeline).
            setError(parseError.isEmpty() ? NetworkErrorMapper::messageFor(currentReply->error())
                                          : parseError);
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
    m_overallTimeout->stop();
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

    const QJsonArray modelArray = ProviderDefaults::isOllamaProvider(provider) ? obj.value("models").toArray() : obj.value("data").toArray();
    for (const QJsonValue& value : modelArray) {
        const QJsonObject modelObj = value.toObject();
        const QString id = ProviderDefaults::isOllamaProvider(provider) ? modelObj.value("name").toString() : modelObj.value("id").toString();
        if (!id.isEmpty() && !models.contains(id)) {
            models.append(id);
        }
    }

    std::sort(models.begin(), models.end(), [](const QString& a, const QString& b) {
        return QString::localeAwareCompare(a, b) < 0;
    });
    return models;
}

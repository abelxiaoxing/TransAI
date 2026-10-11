#include "translation/TranslationService.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

#include "providers/ProviderDefaults.h"
#include "providers/ProviderEndpointBuilder.h"
#include "providers/ThinkingPolicy.h"
#include "translation/PromptBuilder.h"

TranslationService::TranslationService(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_isRequesting(false)
{
    // Abort requests that see no data for 30s so the UI never gets stuck in
    // the requesting state. The timeout resets whenever data keeps streaming.
    m_networkManager->setTransferTimeout(30 * 1000);
}

QString TranslationService::responseData() const
{
    return m_responseData;
}

QString TranslationService::responseError() const
{
    return m_responseError;
}

bool TranslationService::isRequesting() const
{
    return m_isRequesting;
}

void TranslationService::translateFromMap(const QVariantMap& requestMap)
{
    translate(translationRequestFromMap(requestMap));
}

void TranslationService::abort()
{
    if (!m_reply) {
        return;
    }
    cancelCurrentRequest();
    emit translationStopped();
}

void TranslationService::cancelCurrentRequest()
{
    if (!m_reply) {
        return;
    }

    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
    m_streamParser.reset();
    setIsRequesting(false);
}

void TranslationService::handleStreamData(QNetworkReply* reply)
{
    if (!reply || m_reply != reply) {
        return;
    }

    const StreamParseResult result = m_streamParser.appendChunk(reply->readAll());
    if (!result.content.isEmpty()) {
        setResponseData(m_responseData + result.content);
    }
    if (result.hasError && !result.errorMessage.isEmpty()) {
        // Record the error but do not emit translationFailed here; the
        // finished handler emits exactly one terminal signal per request.
        setResponseError(result.errorMessage);
    }
}

void TranslationService::translate(const TranslationRequest& request)
{
    cancelCurrentRequest();

    TranslationRequest normalizedRequest = request;
    normalizedRequest.provider = ProviderDefaults::normalizedProviderName(normalizedRequest.provider);

    if (normalizedRequest.apiServer.trimmed().isEmpty()) {
        normalizedRequest.apiServer = ProviderDefaults::defaultApiServerForProvider(normalizedRequest.provider);
    }

    setResponseError("");
    setResponseData("");

    if (normalizedRequest.text.trimmed().isEmpty()) {
        setIsRequesting(false);
        setResponseError("Please enter text to translate");
        emit translationFailed(m_responseError);
        return;
    }

    if (normalizedRequest.model.trimmed().isEmpty()) {
        setIsRequesting(false);
        setResponseError("Please select a model");
        emit translationFailed(m_responseError);
        return;
    }

    if (normalizedRequest.provider != "ollama" && !ProviderDefaults::isPlausibleApiKey(normalizedRequest.apiKey)) {
        setIsRequesting(false);
        setResponseError(ProviderDefaults::apiKeyErrorMessage());
        emit translationFailed(m_responseError);
        return;
    }

    ProviderEndpointBuilder endpointBuilder;
    QNetworkRequest networkRequest(endpointBuilder.buildChatCompletionUrl(normalizedRequest.apiServer, normalizedRequest.provider));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    if (normalizedRequest.provider != "ollama") {
        networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(normalizedRequest.apiKey.trimmed()).toUtf8());
    }

    PromptBuilder promptBuilder;

    // Decide how to suppress/minimize model thinking for this endpoint/model
    // (e.g. thinking.type=disabled for DeepSeek chat, enable_thinking=false
    // for Qwen3, think=false for Ollama, reasoning_effort=low for o-series).
    ThinkingPolicy thinkingPolicy = ThinkingPolicyBuilder::build(
        normalizedRequest.provider, normalizedRequest.apiServer, normalizedRequest.model, normalizedRequest.thinkingMode);
    if (normalizedRequest.thinkingMode == "auto" && normalizedRequest.mode == TranslationMode::Translate) {
        // Translation is a deterministic task: always instruct the model
        // (prompt level) to skip its reasoning process.
        thinkingPolicy.directAnswerHint = true;
    }

    QJsonObject requestData;
    requestData.insert("model", normalizedRequest.model);
    requestData.insert("stream", true);
    for (auto it = thinkingPolicy.bodyParams.constBegin(); it != thinkingPolicy.bodyParams.constEnd(); ++it) {
        requestData.insert(it.key(), it.value());
    }
    requestData.insert("messages", promptBuilder.buildMessages(normalizedRequest, thinkingPolicy));

    m_streamParser.reset();
    setIsRequesting(true);
    emit translationStarted();

    m_reply = m_networkManager->post(networkRequest, QJsonDocument(requestData).toJson());
    QNetworkReply* currentReply = m_reply;
    connect(currentReply, &QNetworkReply::readyRead, this, [this, currentReply]() {
        handleStreamData(currentReply);
    });
    connect(currentReply, &QNetworkReply::finished, this, [this, currentReply]() {
        if (m_reply != currentReply) {
            currentReply->deleteLater();
            return;
        }

        StreamParseResult result = m_streamParser.appendChunk(currentReply->readAll());
        const StreamParseResult finalResult = m_streamParser.flush();
        result.content += finalResult.content;
        if (finalResult.hasError) {
            result.hasError = true;
            if (!finalResult.errorMessage.isEmpty()) {
                result.errorMessage = finalResult.errorMessage;
            }
        }

        if (!result.content.isEmpty()) {
            setResponseData(m_responseData + result.content);
        }
        if (result.hasError && !result.errorMessage.isEmpty()) {
            setResponseError(result.errorMessage);
        }

        setIsRequesting(false);

        if (currentReply->error() == QNetworkReply::NoError) {
            if (m_responseError.isEmpty()) {
                emit translationFinished(m_responseData);
            } else {
                emit translationFailed(m_responseError);
            }
        } else if (currentReply->error() == QNetworkReply::OperationCanceledError) {
            // abort() disconnects the reply before cancelling it, so a canceled
            // reply that still reaches finished can only be the 30s transfer
            // timeout. Report it instead of leaving the UI without a terminal
            // signal (the reply would otherwise end in a silent "Ready").
            const QString timeoutError = m_responseError.isEmpty()
                ? QStringLiteral("Translation timed out: no data from the server")
                : m_responseError;
            setResponseError(timeoutError);
            emit translationFailed(timeoutError);
        } else {
            const QString error = m_responseError.isEmpty()
                ? networkErrorMessage(currentReply->error())
                : m_responseError;
            setResponseError(error);
            emit translationFailed(error);
        }

        currentReply->deleteLater();
        m_reply = nullptr;
    });
}

void TranslationService::setResponseData(const QString& responseData)
{
    if (m_responseData == responseData) {
        return;
    }
    m_responseData = responseData;
    emit responseDataChanged();
}

void TranslationService::setResponseError(const QString& responseError)
{
    if (m_responseError == responseError) {
        return;
    }
    m_responseError = responseError;
    emit responseErrorChanged();
}

void TranslationService::setIsRequesting(bool isRequesting)
{
    if (m_isRequesting == isRequesting) {
        return;
    }
    m_isRequesting = isRequesting;
    emit isRequestingChanged();
}

QString TranslationService::networkErrorMessage(QNetworkReply::NetworkError error) const
{
    // QNetworkReply error-code ranges (see QNetworkReply documentation):
    //   1-99 network, 101-199 proxy, 201-299 content (covers HTTP 401/403/404),
    //   301-399 protocol, 401-499 server-side.
    if (error > 0 && error < 100) {
        return "Network error: could not reach the server";
    }
    if (error > 100 && error < 200) {
        return "Proxy error: could not reach the server through the proxy";
    }
    if (error > 200 && error < 300) {
        return "Request rejected: check the API key, model name and server URL";
    }
    if (error > 300 && error < 400) {
        return "Protocol error: unexpected response from the server";
    }
    if (error > 400 && error < 500) {
        return "Server error: the API server failed to handle the request";
    }
    return "Network error";
}

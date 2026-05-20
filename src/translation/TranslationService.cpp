#include "translation/TranslationService.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

#include "providers/ProviderEndpointBuilder.h"
#include "translation/PromptBuilder.h"

TranslationService::TranslationService(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_isRequesting(false)
{
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
    cancelCurrentRequest();
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
        setResponseError(result.errorMessage);
        emit translationFailed(result.errorMessage);
    }
}

void TranslationService::translate(const TranslationRequest& request)
{
    cancelCurrentRequest();

    TranslationRequest normalizedRequest = request;
    normalizedRequest.provider = normalizedRequest.provider == "ollama" ? "ollama" : "openai";

    if (normalizedRequest.apiServer.trimmed().isEmpty()) {
        normalizedRequest.apiServer = normalizedRequest.provider == "ollama"
            ? "http://localhost:11434"
            : "https://api.openai.com";
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

    if (normalizedRequest.provider != "ollama" && normalizedRequest.apiKey.trimmed().length() < 10) {
        setIsRequesting(false);
        setResponseError("Please provide the correct apikey");
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
    QJsonObject requestData;
    requestData.insert("model", normalizedRequest.model);
    requestData.insert("stream", true);
    requestData.insert("messages", promptBuilder.buildMessages(normalizedRequest));

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
        result.done = result.done || finalResult.done;
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
        } else if (currentReply->error() != QNetworkReply::OperationCanceledError) {
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
    if (error > 0 && error < 100) {
        return "network error";
    }
    if (error > 100 && error < 200) {
        return "proxy error";
    }
    if (error > 200 && error < 300) {
        return "content error";
    }
    if (error > 300 && error < 400) {
        return "protocol error";
    }
    if (error > 400 && error < 500) {
        return "server error";
    }
    return "network error";
}

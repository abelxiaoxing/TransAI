#include "vision/VisionTranslationService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

#include "providers/ProviderDefaults.h"
#include "providers/ProviderEndpointBuilder.h"
#include "providers/ThinkingPolicy.h"
#include "vision/ImagePayloadBuilder.h"

namespace {
const char kVisionUnsupportedGuidance[] =
    "The selected model may not support image input. "
    "Open Settings and choose a vision-capable model for screenshot translation.";

QString effectiveTargetLanguage(const QString& targetLanguage)
{
    const QString trimmed = targetLanguage.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("English") : trimmed;
}
} // namespace

VisionTranslationService::VisionTranslationService(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_isRequesting(false)
{
    // Same inactivity policy as the text path: a request that sees no data
    // for 30s is aborted so the UI never gets stuck in the requesting state.
    m_networkManager->setTransferTimeout(30 * 1000);
}

QString VisionTranslationService::responseData() const
{
    return m_responseData;
}

QString VisionTranslationService::responseError() const
{
    return m_responseError;
}

bool VisionTranslationService::isRequesting() const
{
    return m_isRequesting;
}

void VisionTranslationService::translateImage(const QImage& image, const QVariantMap& options)
{
    cancelCurrentRequest();

    VisionRequest request;
    request.targetLanguage = effectiveTargetLanguage(options.value(QStringLiteral("targetLanguage")).toString());
    request.provider = ProviderDefaults::normalizedProviderName(options.value(QStringLiteral("provider")).toString());
    request.apiServer = options.value(QStringLiteral("apiServer")).toString();
    if (request.apiServer.trimmed().isEmpty()) {
        request.apiServer = ProviderDefaults::defaultApiServerForProvider(request.provider);
    }
    request.apiKey = options.value(QStringLiteral("apiKey")).toString();
    request.model = options.value(QStringLiteral("model")).toString().trimmed();

    setResponseError("");
    setResponseData("");

    // cancelCurrentRequest() above already reset isRequesting, so the
    // validation failures below only record the error and emit the terminal
    // signal.
    if (image.isNull()) {
        setResponseError(QStringLiteral("No screenshot image was provided"));
        emit translationFailed(m_responseError);
        return;
    }
    if (request.model.isEmpty()) {
        setResponseError(QStringLiteral("Please select a model"));
        emit translationFailed(m_responseError);
        return;
    }
    if (request.provider != "ollama" && !ProviderDefaults::isPlausibleApiKey(request.apiKey)) {
        setResponseError(ProviderDefaults::apiKeyErrorMessage());
        emit translationFailed(m_responseError);
        return;
    }

    const ImagePayloadResult payload = ImagePayloadBuilder::buildDataUrl(
        image,
        options.value(QStringLiteral("maxImageWidth")).toInt(),
        options.value(QStringLiteral("jpegQuality")).toInt());
    if (!payload.ok) {
        setResponseError(payload.error);
        emit translationFailed(m_responseError);
        return;
    }
    request.imageDataUrl = payload.dataUrl;

    // Fresh retry budget per user-facing request.
    m_thinkingParamsAutoDisabled = false;

    m_streamParser.reset();
    setIsRequesting(true);
    emit translationStarted();

    sendChatRequest(request);
}

void VisionTranslationService::abort()
{
    if (!m_reply) {
        return;
    }
    cancelCurrentRequest();
    emit translationStopped();
}

void VisionTranslationService::cancelCurrentRequest()
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

void VisionTranslationService::handleStreamData(QNetworkReply* reply)
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

void VisionTranslationService::sendChatRequest(const VisionRequest& request)
{
    // Same automatic thinking suppression as the text path; translation is a
    // deterministic task, so the prompt-level direct-answer hint is always on.
    ThinkingPolicy thinkingPolicy = ThinkingPolicyBuilder::build(
        request.provider, request.apiServer, request.model);
    thinkingPolicy.directAnswerHint = true;
    if (m_thinkingParamsAutoDisabled) {
        // Transparent retry after the endpoint rejected the thinking fields.
        thinkingPolicy.bodyParams = QJsonObject();
    }

    QNetworkRequest networkRequest(ProviderEndpointBuilder::buildChatCompletionUrl(request.apiServer, request.provider));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    if (request.provider != "ollama") {
        networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(request.apiKey.trimmed()).toUtf8());
    }

    // Vision instruction lives in this module on purpose: image payloads
    // never go through PromptBuilder (text-only).
    QString systemPrompt = QStringLiteral(
        "Extract the text from the image and translate it into %1. "
        "Preserve the original line breaks and layout. Only return the translated result; "
        "do not describe the image, do not add explanations. "
        "If the image contains no readable text, reply with a single line: [no text recognized]")
        .arg(request.targetLanguage);
    if (thinkingPolicy.noThinkTag) {
        systemPrompt += QStringLiteral(" /no_think");
    }
    if (thinkingPolicy.directAnswerHint) {
        systemPrompt += QStringLiteral(
            " Respond directly with the final answer only. "
            "Do not show any thinking, reasoning, chain-of-thought or analysis steps.");
    }

    QJsonObject userText;
    userText.insert(QStringLiteral("type"), QStringLiteral("text"));
    userText.insert(QStringLiteral("text"), QStringLiteral("Translate the text in this image."));

    QJsonObject imageUrl;
    imageUrl.insert(QStringLiteral("url"), QString::fromLatin1(request.imageDataUrl));
    QJsonObject imageContent;
    imageContent.insert(QStringLiteral("type"), QStringLiteral("image_url"));
    imageContent.insert(QStringLiteral("image_url"), imageUrl);

    QJsonObject systemMessage;
    systemMessage.insert(QStringLiteral("role"), QStringLiteral("system"));
    systemMessage.insert(QStringLiteral("content"), systemPrompt);
    QJsonObject userMessage;
    userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMessage.insert(QStringLiteral("content"), QJsonArray{userText, imageContent});

    QJsonObject requestData;
    requestData.insert(QStringLiteral("model"), request.model);
    requestData.insert(QStringLiteral("stream"), true);
    for (auto it = thinkingPolicy.bodyParams.constBegin(); it != thinkingPolicy.bodyParams.constEnd(); ++it) {
        requestData.insert(it.key(), it.value());
    }
    requestData.insert(QStringLiteral("messages"), QJsonArray{systemMessage, userMessage});

    m_reply = m_networkManager->post(networkRequest, QJsonDocument(requestData).toJson());
    QNetworkReply* currentReply = m_reply;
    connect(currentReply, &QNetworkReply::readyRead, this, [this, currentReply]() {
        handleStreamData(currentReply);
    });
    connect(currentReply, &QNetworkReply::finished, this, [this, currentReply, request]() {
        if (m_reply != currentReply) {
            currentReply->deleteLater();
            return;
        }

        // Final read plus the buffered tail, merged into a single result.
        const StreamParseResult result = m_streamParser.consume(currentReply->readAll());

        if (!result.content.isEmpty()) {
            setResponseData(m_responseData + result.content);
        }
        if (result.hasError && !result.errorMessage.isEmpty()) {
            setResponseError(result.errorMessage);
        }

        // Retry once without the thinking fields when a strict endpoint
        // rejects them (same policy as the text path).
        if (!m_thinkingParamsAutoDisabled && m_responseData.isEmpty() && !m_responseError.isEmpty()
            && ThinkingPolicyBuilder::isThinkingParamRejection(m_responseError)) {
            m_thinkingParamsAutoDisabled = true;
            m_streamParser.reset();
            setResponseError("");
            currentReply->deleteLater();
            m_reply = nullptr;
            sendChatRequest(request);
            return;
        }

        setIsRequesting(false);

        QString terminalError;
        bool finished = false;
        if (currentReply->error() == QNetworkReply::NoError) {
            if (m_responseError.isEmpty()) {
                finished = true;
            } else {
                terminalError = m_responseError;
            }
        } else if (currentReply->error() == QNetworkReply::OperationCanceledError) {
            // A canceled reply that still reaches finished can only be the
            // 30s transfer timeout (abort() disconnects its reply first).
            terminalError = m_responseError.isEmpty()
                ? QStringLiteral("Translation timed out: no data from the server")
                : m_responseError;
        } else {
            terminalError = m_responseError.isEmpty()
                ? networkErrorMessage(currentReply->error())
                : m_responseError;
        }

        if (finished) {
            emit translationFinished(m_responseData);
        } else {
            // Turn raw "image not supported" API errors into guidance.
            if (isVisionUnsupportedError(terminalError)) {
                terminalError = QStringLiteral("%1\n\n%2").arg(QLatin1String(kVisionUnsupportedGuidance), terminalError);
            }
            setResponseError(terminalError);
            emit translationFailed(terminalError);
        }

        currentReply->deleteLater();
        m_reply = nullptr;
    });
}

bool VisionTranslationService::isVisionUnsupportedError(const QString& errorMessage)
{
    const QString msg = errorMessage.toLower();
    const bool mentionsImage = msg.contains(QLatin1String("image")) || msg.contains(QLatin1String("vision"))
        || msg.contains(QLatin1String("multimodal")) || msg.contains(QLatin1String("modalit"));
    const bool mentionsRejection = msg.contains(QLatin1String("support")) || msg.contains(QLatin1String("invalid"))
        || msg.contains(QLatin1String("unable")) || msg.contains(QLatin1String("not allowed"))
        || msg.contains(QLatin1String("unrecognized")) || msg.contains(QLatin1String("unknown"));
    return mentionsImage && mentionsRejection;
}

void VisionTranslationService::setResponseData(const QString& responseData)
{
    if (m_responseData == responseData) {
        return;
    }
    m_responseData = responseData;
    emit responseDataChanged();
}

void VisionTranslationService::setResponseError(const QString& responseError)
{
    if (m_responseError == responseError) {
        return;
    }
    m_responseError = responseError;
    emit responseErrorChanged();
}

void VisionTranslationService::setIsRequesting(bool isRequesting)
{
    if (m_isRequesting == isRequesting) {
        return;
    }
    m_isRequesting = isRequesting;
    emit isRequestingChanged();
}

QString VisionTranslationService::networkErrorMessage(QNetworkReply::NetworkError error) const
{
    // Mirrors TranslationService::networkErrorMessage (see its comment for
    // the QNetworkReply error-code ranges).
    if (error > 0 && error < 100) {
        return QStringLiteral("Network error: could not reach the server");
    }
    if (error > 100 && error < 200) {
        return QStringLiteral("Proxy error: could not reach the server through the proxy");
    }
    if (error > 200 && error < 300) {
        return QStringLiteral("Request rejected: check the API key, model name and server URL");
    }
    if (error > 300 && error < 400) {
        return QStringLiteral("Protocol error: unexpected response from the server");
    }
    if (error > 400 && error < 500) {
        return QStringLiteral("Server error: the API server failed to handle the request");
    }
    return QStringLiteral("Network error");
}

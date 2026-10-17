#include "vision/VisionTranslationService.h"

#include <QJsonArray>
#include <QJsonObject>

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
    : StreamingChatClient(parent)
{
}

void VisionTranslationService::translateImage(const QImage& image, const QString& targetLanguage,
                                              const QString& provider, const QString& apiServer,
                                              const QString& apiKey, const QString& model,
                                              int maxImageWidth, int jpegQuality)
{
    VisionRequest request;
    request.targetLanguage = effectiveTargetLanguage(targetLanguage);
    request.provider = ProviderDefaults::normalizedProviderName(provider);
    request.apiServer = apiServer;
    if (request.apiServer.trimmed().isEmpty()) {
        request.apiServer = ProviderDefaults::defaultApiServerForProvider(request.provider);
    }
    request.apiKey = apiKey;
    request.model = model.trimmed();

    if (image.isNull()) {
        failValidation(QStringLiteral("No screenshot image was provided"));
        return;
    }
    if (request.model.isEmpty()) {
        failValidation(QStringLiteral("Please select a model"));
        return;
    }
    if (request.provider != "ollama" && !ProviderDefaults::isPlausibleApiKey(request.apiKey)) {
        failValidation(ProviderDefaults::apiKeyErrorMessage());
        return;
    }

    const ImagePayloadResult payload = ImagePayloadBuilder::buildDataUrl(
        image, maxImageWidth, jpegQuality);
    if (!payload.ok) {
        failValidation(payload.error);
        return;
    }
    request.imageDataUrl = payload.dataUrl;

    startRequest(buildPendingRequest(request));
}

VisionTranslationService::PendingRequest VisionTranslationService::buildPendingRequest(const VisionRequest& request) const
{
    // Same automatic thinking suppression as the text path; translation is a
    // deterministic task, so the prompt-level direct-answer hint is always on.
    ThinkingPolicy thinkingPolicy = ThinkingPolicyBuilder::build(
        request.provider, request.apiServer, request.model);
    thinkingPolicy.directAnswerHint = true;

    PendingRequest pending;
    pending.networkRequest = QNetworkRequest(
        ProviderEndpointBuilder::buildChatCompletionUrl(request.apiServer, request.provider));
    pending.networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    pending.networkRequest.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    if (request.provider != "ollama") {
        pending.networkRequest.setRawHeader(
            "Authorization", QString("Bearer %1").arg(request.apiKey.trimmed()).toUtf8());
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

    const QJsonArray messages{systemMessage, userMessage};

    pending.body.insert(QStringLiteral("model"), request.model);
    pending.body.insert(QStringLiteral("stream"), true);
    for (auto it = thinkingPolicy.bodyParams.constBegin(); it != thinkingPolicy.bodyParams.constEnd(); ++it) {
        pending.body.insert(it.key(), it.value());
    }
    pending.body.insert(QStringLiteral("messages"), messages);

    // Transparent-retry fallback for endpoints that reject the thinking
    // fields: identical body minus those fields (equal when none were added).
    pending.bodyWithoutThinking = pending.body;
    for (auto it = thinkingPolicy.bodyParams.constBegin(); it != thinkingPolicy.bodyParams.constEnd(); ++it) {
        pending.bodyWithoutThinking.remove(it.key());
    }

    return pending;
}

QString VisionTranslationService::decorateFailureMessage(const QString& error) const
{
    // Turn raw "image not supported" API errors into guidance.
    if (isVisionUnsupportedError(error)) {
        return QStringLiteral("%1\n\n%2").arg(QLatin1String(kVisionUnsupportedGuidance), error);
    }
    return error;
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

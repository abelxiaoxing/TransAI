#include "translation/TranslationService.h"

#include <QJsonObject>

#include "providers/ProviderDefaults.h"
#include "providers/ProviderEndpointBuilder.h"
#include "providers/ThinkingPolicy.h"
#include "translation/PromptBuilder.h"

TranslationService::TranslationService(QObject* parent)
    : StreamingChatClient(parent)
{
}

void TranslationService::translate(const QString& text, const QString& targetLanguage, int mode,
                                   const QString& provider, const QString& apiServer,
                                   const QString& apiKey, const QString& model)
{
    TranslationRequest request;
    request.text = text;
    request.targetLanguage = targetLanguage;
    request.mode = translationModeFromInt(mode);
    request.provider = provider;
    request.apiServer = apiServer;
    request.apiKey = apiKey;
    request.model = model;
    translate(request);
}

void TranslationService::translate(const TranslationRequest& request)
{
    TranslationRequest normalizedRequest = request;
    normalizedRequest.provider = ProviderDefaults::normalizedProviderName(normalizedRequest.provider);

    if (normalizedRequest.apiServer.trimmed().isEmpty()) {
        normalizedRequest.apiServer = ProviderDefaults::defaultApiServerForProvider(normalizedRequest.provider);
    }

    if (normalizedRequest.text.trimmed().isEmpty()) {
        failValidation(QStringLiteral("Please enter text to translate"));
        return;
    }

    if (normalizedRequest.model.trimmed().isEmpty()) {
        failValidation(QStringLiteral("Please select a model"));
        return;
    }

    if (normalizedRequest.provider != "ollama" && !ProviderDefaults::isPlausibleApiKey(normalizedRequest.apiKey)) {
        failValidation(ProviderDefaults::apiKeyErrorMessage());
        return;
    }

    startRequest(buildPendingRequest(normalizedRequest));
}

TranslationService::PendingRequest TranslationService::buildPendingRequest(const TranslationRequest& request) const
{
    // Decide how to suppress/minimize model thinking for this endpoint/model
    // (e.g. thinking.type=disabled for DeepSeek chat, enable_thinking=false
    // for Qwen3, think=false for Ollama, reasoning_effort=low for o-series).
    // Fully automatic: the policy is derived from provider/host/model only.
    ThinkingPolicy thinkingPolicy = ThinkingPolicyBuilder::build(
        request.provider, request.apiServer, request.model);
    if (request.mode == TranslationMode::Translate) {
        // Translation is a deterministic task: always instruct the model
        // (prompt level) to skip its reasoning process.
        thinkingPolicy.directAnswerHint = true;
    }

    PendingRequest pending;
    pending.networkRequest = QNetworkRequest(
        ProviderEndpointBuilder::buildChatCompletionUrl(request.apiServer, request.provider));
    pending.networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    pending.networkRequest.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    if (request.provider != "ollama") {
        pending.networkRequest.setRawHeader(
            "Authorization", QString("Bearer %1").arg(request.apiKey.trimmed()).toUtf8());
    }

    const QJsonValue messages = PromptBuilder::buildMessages(request, thinkingPolicy);

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

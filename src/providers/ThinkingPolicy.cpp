#include "providers/ThinkingPolicy.h"

#include <QRegularExpression>
#include <QUrl>

#include "providers/ProviderDefaults.h"

namespace {

bool containsAny(const QString& value, const QStringList& needles)
{
    const QString lowered = value.toLower();
    for (const QString& needle : needles) {
        if (lowered.contains(needle)) {
            return true;
        }
    }
    return false;
}

bool isDeepSeekModel(const QString& model)
{
    return model.startsWith(QLatin1String("deepseek"), Qt::CaseInsensitive);
}

bool isDeepSeekReasonerModel(const QString& model)
{
    // deepseek-reasoner / deepseek-r1 / deepseek-r1-0528 / deepseek-r2 ...
    const QString m = model.toLower();
    return m.contains(QLatin1String("reasoner")) || m.contains(QLatin1String("r1")) || m.contains(QLatin1String("r2"));
}

bool isQwenReasoningModel(const QString& model)
{
    // Qwen3 family (including QwQ and *-thinking variants)
    const QString m = model.toLower();
    return m.contains(QLatin1String("qwen3")) || m.contains(QLatin1String("qwq"))
        || (m.contains(QLatin1String("qwen")) && m.contains(QLatin1String("think")));
}

bool isOpenAiReasoningModel(const QString& model)
{
    // o1 / o1-mini / o3 / o3-mini / o4-mini / gpt-5 ...
    static const QRegularExpression openaiReasoning("^(o1|o3|o4)(-|\\.|_|$)|^gpt-5");
    return openaiReasoning.match(model.toLower()).hasMatch();
}

bool looksLikeThinkingModel(const QString& model)
{
    const QString m = model.toLower();
    return containsAny(m, { "think", "reason", "qwq", "r1", "minimax" }) || isOpenAiReasoningModel(m);
}

// Qwen hybrid-thinking models (Qwen Cloud / DashScope OpenAI-compatible
// endpoints, self-hosted vLLM/SGLang servers):
//  - the top-level `enable_thinking: false` field (their docs explicitly ask
//    for the top-level field, not chat_template_kwargs),
//  - the /no_think prompt tag, a documented per-turn override that is safe on
//    every Qwen endpoint.
// Sending both layers at once guarantees suppression even when the upstream
// only honors one of them; unknown fields are ignored by the rest (and the
// rejected-parameter retry in TranslationService covers strict endpoints).
void applyQwenPolicy(ThinkingPolicy& policy)
{
    policy.bodyParams.insert("enable_thinking", false);
    policy.noThinkTag = true;
}

} // namespace

ThinkingPolicy ThinkingPolicyBuilder::build(const QString& provider, const QString& apiServer, const QString& model)
{
    ThinkingPolicy policy;

    const QString m = model.toLower();
    const QString normalizedProvider = provider.trimmed().toLower();
    const QString host = QUrl(apiServer).host().toLower();

    // 1) Ollama: `think: false` disables thinking on supported models
    //    (Qwen3/3.5+, DeepSeek-R1, GPT-OSS, GLM, Gemma3, ...) and is ignored
    //    by models without thinking support.
    if (ProviderDefaults::isOllamaProvider(normalizedProvider)) {
        policy.bodyParams.insert("think", false);
        return policy;
    }

    // 2) OpenRouter: cap the reasoning effort to "low" for thinking models.
    //    OpenRouter normalizes and ignores the parameter for models without
    //    reasoning support; non-matching aliases are left untouched.
    if (host.contains(QLatin1String("openrouter.ai"))) {
        if (looksLikeThinkingModel(m) || isQwenReasoningModel(m) || isDeepSeekModel(m)) {
            policy.bodyParams.insert("reasoning", QJsonObject({ { "effort", "low" } }));
        }
        return policy;
    }

    // 3) DeepSeek:
    //    - chat / V3 / V4 models accept `thinking.type = "disabled"`.
    //    - reasoner (R1/R2) models cannot be disabled through API parameters;
    //      R1-0528+ honors the /no_think prompt tag instead.
    if (isDeepSeekModel(m)) {
        if (isDeepSeekReasonerModel(m)) {
            policy.noThinkTag = true;
        } else {
            policy.bodyParams.insert("thinking", QJsonObject({ { "type", "disabled" } }));
        }
        return policy;
    }

    // 4) Qwen hybrid-thinking models (Qwen Cloud / DashScope compatible
    //    endpoints and self-hosted vLLM/SGLang servers).
    if (isQwenReasoningModel(m)) {
        applyQwenPolicy(policy);
        return policy;
    }

    // 5) OpenAI o-series / GPT-5: minimize the reasoning effort.
    if (isOpenAiReasoningModel(m)) {
        policy.bodyParams.insert("reasoning_effort", "low");
        return policy;
    }

    // 6) Fallback: prompt-level nudge for any remaining thinking model.
    //    Prompt instructions never break the request.
    if (looksLikeThinkingModel(m)) {
        policy.directAnswerHint = true;
    }

    return policy;
}

bool ThinkingPolicyBuilder::isThinkingParamRejection(const QString& errorMessage)
{
    if (errorMessage.trimmed().isEmpty()) {
        return false;
    }

    // The thinking-related request fields this app may add (lowercase;
    // "think" also matches "thinking" / "enable_thinking" mentions).
    static const QStringList kParamNames = {
        QStringLiteral("enable_thinking"), QStringLiteral("reasoning_effort"),
        QStringLiteral("reasoning"), QStringLiteral("thinking"), QStringLiteral("think"),
    };

    // Error-message phrases that indicate a request field was rejected as
    // unknown/unsupported (FastAPI/pydantic relays, OpenAI, generic gateways).
    static const QStringList kRejectionPhrases = {
        QStringLiteral("unknown parameter"),
        QStringLiteral("unexpected parameter"),
        QStringLiteral("unrecognized"),
        QStringLiteral("extra fields not permitted"),
        QStringLiteral("extra inputs are not permitted"),
        QStringLiteral("extra_forbidden"),
        QStringLiteral("invalid parameter"),
        QStringLiteral("unsupported parameter"),
        QStringLiteral("not supported"),
        QStringLiteral("does not support"),
        QStringLiteral("not allowed"),
    };

    // Both a parameter name and a rejection phrase must appear: a bare
    // rejection phrase (e.g. "streaming not supported") must not trigger a
    // retry, and a bare parameter mention alone proves nothing either.
    return containsAny(errorMessage, kParamNames) && containsAny(errorMessage, kRejectionPhrases);
}

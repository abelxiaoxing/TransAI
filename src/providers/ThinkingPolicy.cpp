#include "providers/ThinkingPolicy.h"

#include <QRegularExpression>
#include <QUrl>

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
// only honors one of them; unknown fields are ignored by the rest.
void applyQwenPolicy(ThinkingPolicy& policy)
{
    policy.bodyParams.insert("enable_thinking", false);
    policy.noThinkTag = true;
}

} // namespace

QString ThinkingPolicyBuilder::normalizeThinkingMode(const QString& mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized == QLatin1String("qwen") || normalized == QLatin1String("deepseek")
        || normalized == QLatin1String("openai") || normalized == QLatin1String("ollama")
        || normalized == QLatin1String("manual")) {
        return normalized;
    }
    if (normalized == QLatin1String("none")) {
        return QLatin1String("manual");
    }
    return QLatin1String("auto");
}

ThinkingPolicy ThinkingPolicyBuilder::build(const QString& provider, const QString& apiServer, const QString& model, const QString& mode)
{
    ThinkingPolicy policy;

    // normalizeThinkingMode is the single validation point for mode strings
    // crossing the config/QVariantMap boundaries; build() relies on it so raw
    // input ("none", empty, casing) gets the same mapping everywhere.
    const QString modeKey = normalizeThinkingMode(mode);
    // Manual mode: send the request untouched.
    if (modeKey == QLatin1String("manual")) {
        return policy;
    }

    const QString m = model.toLower();
    const QString normalizedProvider = provider.trimmed().toLower();

    // Forced vendor profiles: apply the vendor's thinking controls regardless
    // of the model id. Needed when the model id is a relay alias (e.g.
    // "LocalModel" on a NewAPI gateway) that hides the real model family.
    if (modeKey != QLatin1String("auto")) {
        if (normalizedProvider == QLatin1String("ollama") || modeKey == QLatin1String("ollama")) {
            // Ollama: `think: false` is understood by the OpenAI-compatible
            // endpoint too and ignored by non-thinking models.
            policy.bodyParams.insert("think", false);
        } else if (modeKey == QLatin1String("qwen")) {
            applyQwenPolicy(policy);
        } else if (modeKey == QLatin1String("deepseek")) {
            if (isDeepSeekReasonerModel(m)) {
                policy.noThinkTag = true;
            } else {
                policy.bodyParams.insert("thinking", QJsonObject({ { "type", "disabled" } }));
                policy.noThinkTag = true;
            }
        } else if (modeKey == QLatin1String("openai")) {
            policy.bodyParams.insert("reasoning_effort", "low");
        }
        // Unknown profile: validated upstream, keep the request untouched.
        return policy;
    }

    const QString host = QUrl(apiServer).host().toLower();

    // 1) Ollama: `think: false` disables thinking on supported models
    //    (Qwen3/3.5+, DeepSeek-R1, GPT-OSS, GLM, Gemma3, ...) and is ignored
    //    by models without thinking support.
    if (normalizedProvider == QLatin1String("ollama")) {
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

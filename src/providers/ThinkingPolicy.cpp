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

bool isQwenNewGenModel(const QString& model)
{
    // Qwen3.5 / 3.6 / 3.7 / 3.8 generation (e.g. "qwen3.8-flash", "qwen3.5-9b").
    // These generations officially expose reasoning_effort (default xhigh!)
    // in addition to enable_thinking.
    return model.toLower().contains(QLatin1String("qwen3."));
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

// Qwen hybrid-thinking models:
//  - Qwen Cloud / DashScope OpenAI-compatible endpoints accept the top-level
//    `enable_thinking: false` (their docs explicitly say to use the top-level
//    field, not chat_template_kwargs).
//  - vLLM / SGLang / Azure Foundry style servers for the Qwen3.5+ generation
//    accept `reasoning_effort` ("low" is the smallest level; the default is
//    "xhigh", i.e. maximum thinking).
//  - The /no_think prompt tag is a documented per-turn override for Qwen
//    hybrid models and is safe on every Qwen endpoint.
// Sending all layers at once guarantees suppression even when the upstream
// only honors one of them; unknown fields are ignored by the rest.
void applyQwenPolicy(ThinkingPolicy& policy, bool newGeneration)
{
    policy.bodyParams.insert("enable_thinking", false);
    if (newGeneration) {
        policy.bodyParams.insert("reasoning_effort", "low");
    }
    policy.noThinkTag = true;
}

} // namespace

ThinkingPolicy ThinkingPolicyBuilder::build(const QString& provider, const QString& apiServer, const QString& model, const QString& mode)
{
    ThinkingPolicy policy;

    const QString modeKey = mode.trimmed().toLower();
    // Manual mode: send the request untouched.
    if (modeKey == QLatin1String("manual") || modeKey == QLatin1String("none") || modeKey.isEmpty()) {
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
            applyQwenPolicy(policy, true);
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
    //    Non-thinking models are left untouched to avoid provider errors.
    if (host.contains(QLatin1String("openrouter.ai"))) {
        if (looksLikeThinkingModel(m)
            || containsAny(m, { "deepseek", "qwen", "gpt-5", "kimi", "claude", "grok", "mistral", "gemini-2.5" })) {
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
        applyQwenPolicy(policy, isQwenNewGenModel(m));
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

#ifndef THINKING_POLICY_H
#define THINKING_POLICY_H

#include <QJsonObject>
#include <QString>

// Describes how to suppress or minimize model "thinking" (reasoning) output
// for a given provider / endpoint / model combination.
//
// Thinking suppression works on two levels:
//  1. API parameters (bodyParams): provider-specific request fields such as
//     {"thinking": {"type": "disabled"}} for DeepSeek chat,
//     {"enable_thinking": false} for Qwen3,
//     {"think": false} for Ollama,
//     {"reasoning_effort": "low"} for OpenAI o-series,
//     {"reasoning": {"effort": "low"}} for OpenRouter.
//     These are only added when the target model is known to accept them.
//  2. Prompt level: an optional "/no_think" tag (DeepSeek R1 reasoners) and/or
//     a "answer directly, no reasoning steps" hint appended to the system
//     prompt. Prompt-level instructions are safe for any model.
struct ThinkingPolicy
{
    // Extra JSON fields merged into the chat-completion request body.
    QJsonObject bodyParams;

    // Append "/no_think" to the system prompt (DeepSeek R1 reasoner models,
    // which cannot be disabled through API parameters).
    bool noThinkTag = false;

    // Append a prompt-level "answer directly, do not show reasoning" hint.
    bool directAnswerHint = false;

    bool isActive() const
    {
        return !bodyParams.isEmpty() || noThinkTag || directAnswerHint;
    }
};

class ThinkingPolicyBuilder
{
public:
    // Canonical thinking modes (the only accepted values, see
    // normalizeThinkingMode):
    //   "auto"    - detect the model family from the model id (recommended, default)
    //   "qwen"    - force Qwen3 controls (enable_thinking=false + /no_think)
    //   "deepseek"- force DeepSeek controls (thinking.type=disabled / /no_think)
    //   "openai"  - force OpenAI o-series controls (reasoning_effort=low)
    //   "ollama"  - force Ollama controls (think=false)
    //   "manual"  - never add thinking-related parameters (request sent as-is)
    //
    // Forced profiles are useful when the model id is a relay alias (e.g.
    // "LocalModel" on a NewAPI gateway) that hides the real model family.
    //
    // `mode` may be raw/un-normalized; it is passed through
    // normalizeThinkingMode internally ("none" -> manual, empty/unknown -> auto).
    static ThinkingPolicy build(const QString& provider, const QString& apiServer, const QString& model, const QString& mode);

    // Single validation point for the thinking-mode string crossing the
    // config file / QVariantMap boundaries. Maps "none" to "manual" and
    // empty or unknown values to "auto".
    static QString normalizeThinkingMode(const QString& mode);
};

#endif // THINKING_POLICY_H

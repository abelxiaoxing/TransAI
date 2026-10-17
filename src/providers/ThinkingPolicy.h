#ifndef THINKING_POLICY_H
#define THINKING_POLICY_H

#include <QJsonObject>
#include <QString>

// Describes how to suppress or minimize model "thinking" (reasoning) output
// for a given provider / endpoint / model combination. Fully automatic: the
// policy is derived from the provider, endpoint host and model id; there is
// deliberately no user-facing setting.
//
// Thinking suppression works on three layers:
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
//  3. Output level (elsewhere): StreamParser strips <think>...</think> blocks
//     that still slip through (relay model aliases, models ignoring the
//     parameters), and isThinkingParamRejection() lets TranslationService
//     retry once without the bodyParams when a strict endpoint rejects them.
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
    // Detects the model family from the model id / endpoint host and returns
    // the matching suppression policy (see the ThinkingPolicy comment for the
    // three layers). Always "auto": unknown models are left to the prompt and
    // output-level layers, which are safe for any model.
    static ThinkingPolicy build(const QString& provider, const QString& apiServer, const QString& model);

    // True when an error message indicates the endpoint rejected one of the
    // thinking-related request fields (e.g. strict OpenAI-compatible relays
    // answer 400 "Extra fields not permitted" / "Unknown parameter" for
    // unknown JSON fields). TranslationService uses it to transparently retry
    // once without the parameters before surfacing the failure.
    static bool isThinkingParamRejection(const QString& errorMessage);
};

#endif // THINKING_POLICY_H

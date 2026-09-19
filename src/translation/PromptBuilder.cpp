#include "translation/PromptBuilder.h"

namespace {
// DeepSeek R1 reasoner models disable their reasoning process when the
// system/user prompt contains this tag (R1-0528+).
const char kNoThinkTag[] = " /no_think";

// Prompt-level fallback for models/endpoints without a thinking parameter:
// ask the model to skip its reasoning process and answer directly.
const char kDirectAnswerHint[] =
    " Respond directly with the final answer only. "
    "Do not show any thinking, reasoning, chain-of-thought or analysis steps.";

QString effectiveTargetLanguage(const QString& targetLanguage)
{
    const QString trimmed = targetLanguage.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("English") : trimmed;
}
} // namespace

QJsonArray PromptBuilder::buildMessages(const TranslationRequest& request, const ThinkingPolicy& policy) const
{
    QJsonArray messages;
    const QString targetLanguage = effectiveTargetLanguage(request.targetLanguage);
    QString systemPrompt;
    QString userMsg;

    switch (request.mode) {
    case TranslationMode::Translate:
        // Tag-style delimiters are far less likely to appear inside user text
        // than triple backticks, and are easier for the model to keep intact.
        systemPrompt = QString("Translate the text enclosed in <text_to_translate> tags into %1. "
                               "Only return the translated result; do not interpret it, "
                               "do not include the tags.").arg(targetLanguage);
        userMsg = "<text_to_translate>" + request.text + "</text_to_translate>";
        break;
    case TranslationMode::ExplainWord:
        systemPrompt = QString(
            "Explain the text enclosed in <text_to_explain> tags. "
            "When the text is a single word, provide: the original form (if applicable), "
            "the language of the word, the corresponding phonetic transcription (if applicable), "
            "all meanings (including parts of speech), and at least three bilingual examples. "
            "Strictly follow this format:\n"
            "<Original Text>\n"
            "[<Language>] · /<Phonetic Transcription>/\n"
            "[<Part of Speech Abbreviation>] <Meaning in %1>\n"
            "Examples:\n"
            "<Number>. <Example> (<Example Translation>)\n"
            "All output text must be written in %1.").arg(targetLanguage);
        userMsg = "<text_to_explain>" + request.text + "</text_to_explain>";
        break;
    case TranslationMode::CorrectGrammar:
        systemPrompt = QString("Strictly correct the grammar mistakes, typos and factual errors "
                               "of the sentence enclosed in <text_to_correct> tags. "
                               "Only correct the sentence; do not add explanations.");
        userMsg = "<text_to_correct>" + request.text + "</text_to_correct>";
        break;
    }

    // Apply thinking suppression at the prompt level (safe for any model).
    if (policy.noThinkTag) {
        systemPrompt += kNoThinkTag;
    }
    if (policy.directAnswerHint) {
        systemPrompt += kDirectAnswerHint;
    }

    messages.append(createMessage("system", systemPrompt));
    messages.append(createMessage("user", userMsg));
    return messages;
}

QJsonObject PromptBuilder::createMessage(const QString& role, const QString& content) const
{
    QJsonObject message;
    message.insert("role", role);
    message.insert("content", content);
    return message;
}

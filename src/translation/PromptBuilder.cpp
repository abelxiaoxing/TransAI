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
}

QJsonArray PromptBuilder::buildMessages(const TranslationRequest& request, const ThinkingPolicy& policy) const
{
    QJsonArray messages;
    QString systemcmd;
    QString userMsg;

    switch (request.mode) {
    case TranslationMode::Translate:
        systemcmd = QString("Translate the text to %1, which is delimited with triple backticks. Only return the translate result, don’t interpret it, don't return the delimited character.").arg(request.targetLanguage);
        userMsg = "translate text:'''" + request.text + "'''";
        break;
    case TranslationMode::ExplainWord:
        systemcmd = QString("Translate anything that I say to %1. When the text contains only one single word, please provide the original form (if applicable), \
  the language of the word, the corresponding phonetic transcription (if applicable), \
  all meanings (including parts of speech), and at least three bilingual examples. Please strictly follow the format below:\
                                             <Original Text> \n \
                                             [<Language>] · / <Phonetic Transcription> \n \
                                             [<Part of Speech Abbreviation>] <Chinese Meaning>] \n \
                                             Examples: \n\
                                             <Number><Example>(Example Translation).The content in this format must be %1 either").arg(request.targetLanguage);
        userMsg = "\"" + request.text + "\"";
        break;
    case TranslationMode::CorrectGrammar:
        systemcmd = QString("I want you to strictly correct my grammar mistakes, typos, and factual errors.Only correct sentence in the brackets.");
        userMsg = " The sentence is: [" + request.text + "]";
        break;
    }

    // Apply thinking suppression at the prompt level (safe for any model).
    if (policy.noThinkTag) {
        systemcmd += kNoThinkTag;
    }
    if (policy.directAnswerHint) {
        systemcmd += kDirectAnswerHint;
    }

    messages.append(createMessage("system", systemcmd));
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

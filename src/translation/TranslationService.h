#ifndef TRANSLATION_SERVICE_H
#define TRANSLATION_SERVICE_H

#include "translation/StreamingChatClient.h"
#include "translation/TranslationRequest.h"

// Streaming text-translation service. All networking, SSE parsing, timeouts
// and the thinking-params retry live in StreamingChatClient; this subclass
// only validates the request and assembles the chat-completion body
// (ThinkingPolicy + PromptBuilder).
class TranslationService : public StreamingChatClient
{
    Q_OBJECT

public:
    explicit TranslationService(QObject* parent = nullptr);

    // Explicit-parameter request API: arity/type mistakes surface at the
    // call site instead of silently missing QVariantMap keys.
    Q_INVOKABLE void translate(const QString& text, const QString& targetLanguage, int mode,
                               const QString& provider, const QString& apiServer,
                               const QString& apiKey, const QString& model);

private:
    void translate(const TranslationRequest& request);
    // Builds the request bodies with and without the automatic thinking
    // parameters (equal when the policy added none).
    PendingRequest buildPendingRequest(const TranslationRequest& request) const;
};

#endif // TRANSLATION_SERVICE_H

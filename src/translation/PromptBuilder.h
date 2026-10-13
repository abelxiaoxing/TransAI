#ifndef PROMPT_BUILDER_H
#define PROMPT_BUILDER_H

#include <QJsonArray>
#include <QJsonObject>

#include "providers/ThinkingPolicy.h"
#include "translation/TranslationRequest.h"

class PromptBuilder
{
public:
    // Stateless helpers (static for parity with ProviderEndpointBuilder and
    // ThinkingPolicyBuilder); there is no reason to instantiate this class.
    static QJsonArray buildMessages(const TranslationRequest& request, const ThinkingPolicy& policy);

private:
    static QJsonObject createMessage(const QString& role, const QString& content);
};

#endif // PROMPT_BUILDER_H

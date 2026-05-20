#ifndef PROMPT_BUILDER_H
#define PROMPT_BUILDER_H

#include <QJsonArray>
#include <QJsonObject>

#include "translation/TranslationRequest.h"

class PromptBuilder
{
public:
    QJsonArray buildMessages(const TranslationRequest& request) const;

private:
    QJsonObject createMessage(const QString& role, const QString& content) const;
};

#endif // PROMPT_BUILDER_H

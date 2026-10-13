#ifndef PROVIDER_ENDPOINT_BUILDER_H
#define PROVIDER_ENDPOINT_BUILDER_H

#include <QUrl>
#include <QString>

class ProviderEndpointBuilder
{
public:
    // Stateless helpers (static for parity with ThinkingPolicyBuilder);
    // there is no reason to instantiate this class.
    static QUrl buildChatCompletionUrl(const QString& apiServer, const QString& provider);
    static QUrl buildModelListUrl(const QString& apiServer, const QString& provider);
};

#endif // PROVIDER_ENDPOINT_BUILDER_H

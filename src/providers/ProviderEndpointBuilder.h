#ifndef PROVIDER_ENDPOINT_BUILDER_H
#define PROVIDER_ENDPOINT_BUILDER_H

#include <QUrl>
#include <QString>

class ProviderEndpointBuilder
{
public:
    QUrl buildChatCompletionUrl(const QString& apiServer, const QString& provider = QString()) const;
    QUrl buildModelListUrl(const QString& apiServer, const QString& provider) const;
};

#endif // PROVIDER_ENDPOINT_BUILDER_H

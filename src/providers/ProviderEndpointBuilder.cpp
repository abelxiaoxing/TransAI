#include "providers/ProviderEndpointBuilder.h"

#include "providers/ProviderDefaults.h"

namespace {

// Resolves the request base URL: explicit server, else the provider default.
QUrl requestUrl(const QString& apiServer, const QString& defaultServer)
{
    const QString trimmed = apiServer.trimmed();
    return QUrl(trimmed.isEmpty() ? defaultServer : trimmed);
}

// Drops trailing slashes so the suffix matching below sees stable paths.
QString trimTrailingSlash(QString path)
{
    while (path.endsWith('/')) {
        path.chop(1);
    }
    return path;
}

// Removes the first matching known endpoint suffix, if any.
void stripKnownSuffix(QString& path, std::initializer_list<const char*> suffixes)
{
    for (const char* suffix : suffixes) {
        if (path.endsWith(QLatin1String(suffix))) {
            path.chop(static_cast<qsizetype>(qstrlen(suffix)));
            return;
        }
    }
}

} // namespace

QUrl ProviderEndpointBuilder::buildChatCompletionUrl(const QString& apiServer, const QString& provider)
{
    const QString normalizedProvider = ProviderDefaults::normalizedProviderName(provider);
    QUrl url = requestUrl(apiServer, ProviderDefaults::defaultApiServerForProvider(normalizedProvider));
    QString path = trimTrailingSlash(url.path());

    if (normalizedProvider == QLatin1String("ollama")) {
        if (path.endsWith(QLatin1String("/v1"))) {
            path += QLatin1String("/chat/completions");
        } else {
            // "/api/tags", "/api/chat" and pasted OpenAI-compatible
            // endpoints ("/v1/chat/completions", "/v1/models") all reduce to
            // the OpenAI-compatible chat endpoint.
            stripKnownSuffix(path, {"/api/tags", "/api/chat", "/v1/chat/completions", "/v1/models", "/models"});
            path += QLatin1String("/v1/chat/completions");
        }
    } else if (path.isEmpty() || path == QLatin1String("/")) {
        path = QLatin1String("/v1/chat/completions");
    } else if (path.endsWith(QLatin1String("/chat/completions"))) {
        // already correct
    } else if (path.endsWith(QLatin1String("/models"))) {
        // User pasted the model-list URL; swap the suffix.
        stripKnownSuffix(path, {"/models"});
        path += QLatin1String("/chat/completions");
    } else if (path.endsWith(QLatin1String("/chat"))) {
        path += QLatin1String("/completions");
    } else {
        // Covers base URLs ("/v1", host root with a path, anything else).
        path += QLatin1String("/chat/completions");
    }

    url.setPath(path);
    url.setQuery(QString());
    return url;
}

QUrl ProviderEndpointBuilder::buildModelListUrl(const QString& apiServer, const QString& provider)
{
    const QString normalizedProvider = ProviderDefaults::normalizedProviderName(provider);
    QUrl url = requestUrl(apiServer, ProviderDefaults::defaultApiServerForProvider(normalizedProvider));
    QString path = trimTrailingSlash(url.path());

    if (normalizedProvider == QLatin1String("ollama")) {
        if (path.endsWith(QLatin1String("/api/tags"))) {
            // already correct
        } else if (path.isEmpty() || path == QLatin1String("/") || path.endsWith(QLatin1String("/v1"))) {
            path = QLatin1String("/api/tags");
        } else {
            // Pasted OpenAI-compatible endpoints (chat completions, model
            // list) reduce to the native tag list.
            stripKnownSuffix(path, {"/v1/chat/completions", "/chat/completions", "/api/chat", "/v1/models", "/models"});
            path += QLatin1String("/api/tags");
        }
    } else {
        stripKnownSuffix(path, {"/chat/completions"});
        if (path.isEmpty() || path == QLatin1String("/")) {
            path = QLatin1String("/v1");
        }
        if (!path.endsWith(QLatin1String("/models"))) {
            path += QLatin1String("/models");
        }
    }

    url.setPath(path);
    url.setQuery(QString());
    return url;
}

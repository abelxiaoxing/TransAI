#include "providers/ProviderEndpointBuilder.h"

namespace {
const char kDefaultOpenAIApiServer[] = "https://api.openai.com";
const char kDefaultOllamaApiServer[] = "http://localhost:11434";

QString normalizeProvider(const QString& provider)
{
    return provider.trimmed().toLower() == "ollama" ? "ollama" : "openai";
}
}

QUrl ProviderEndpointBuilder::buildChatCompletionUrl(const QString& apiServer, const QString& provider) const
{
    const QString normalizedProvider = normalizeProvider(provider);
    const QString defaultServer = normalizedProvider == "ollama" ? kDefaultOllamaApiServer : kDefaultOpenAIApiServer;
    QUrl url(apiServer.trimmed().isEmpty() ? defaultServer : apiServer.trimmed());
    QString path = url.path().trimmed();

    if (normalizedProvider == "ollama") {
        if (path.isEmpty() || path == "/") {
            path = "/v1/chat/completions";
        } else {
            if (path.endsWith('/')) {
                path.chop(1);
            }
            if (path.endsWith("/api/tags")) {
                path = path.left(path.length() - QString("/api/tags").length()) + "/v1/chat/completions";
            } else if (path.endsWith("/api/chat")) {
                path = path.left(path.length() - QString("/api/chat").length()) + "/v1/chat/completions";
            } else if (path.endsWith("/v1")) {
                path += "/chat/completions";
            } else if (!path.endsWith("/chat/completions")) {
                path += "/v1/chat/completions";
            }
        }
    } else if (path.isEmpty() || path == "/") {
        path = "/v1/chat/completions";
    } else {
        if (path.endsWith('/')) {
            path.chop(1);
        }

        if (path.endsWith("/chat/completions")) {
            // already correct
        } else if (path.endsWith("/models")) {
            path = path.left(path.length() - QString("/models").length()) + "/chat/completions";
        } else if (path.endsWith("/v1")) {
            path += "/chat/completions";
        } else if (path.endsWith("/chat")) {
            path += "/completions";
        } else {
            path += "/chat/completions";
        }
    }

    url.setPath(path);
    url.setQuery(QString());
    return url;
}

QUrl ProviderEndpointBuilder::buildModelListUrl(const QString& apiServer, const QString& provider) const
{
    if (normalizeProvider(provider) == "ollama") {
        QUrl url(apiServer.trimmed().isEmpty() ? kDefaultOllamaApiServer : apiServer.trimmed());
        QString path = url.path();
        if (path.isEmpty() || path == "/" || path == "/v1") {
            path = "/api/tags";
        } else if (path.contains("/v1/chat/completions")) {
            path.replace("/v1/chat/completions", "/api/tags");
        } else if (path.contains("/chat/completions")) {
            path.replace("/chat/completions", "/api/tags");
        } else if (path.contains("/api/chat")) {
            path.replace("/api/chat", "/api/tags");
        } else if (!path.endsWith("/api/tags")) {
            if (path.endsWith('/')) {
                path.chop(1);
            }
            path += "/api/tags";
        }
        url.setPath(path);
        url.setQuery(QString());
        return url;
    }

    QUrl url(apiServer.trimmed().isEmpty() ? kDefaultOpenAIApiServer : apiServer.trimmed());
    QString path = url.path();
    if (path.endsWith("/chat/completions")) {
        path = path.left(path.length() - QString("/chat/completions").length());
    }
    if (path.isEmpty() || path == "/") {
        path = "/v1";
    }
    if (!path.endsWith("/models")) {
        if (path.endsWith('/')) {
            path.chop(1);
        }
        path += "/models";
    }
    url.setPath(path);
    url.setQuery(QString());
    return url;
}

#ifndef PROVIDER_DEFAULTS_H
#define PROVIDER_DEFAULTS_H

#include <QString>

// Single source of truth for provider-related defaults shared by the
// settings store, the translation service and the model discovery service.
namespace ProviderDefaults {

inline QString openAiApiServer()
{
    return QStringLiteral("https://api.openai.com");
}

inline QString ollamaApiServer()
{
    return QStringLiteral("http://localhost:11434");
}

inline QString normalizedProviderName(const QString& provider)
{
    return provider.trimmed().toLower() == QLatin1String("ollama")
        ? QStringLiteral("ollama")
        : QStringLiteral("openai");
}

inline QString defaultApiServerForProvider(const QString& provider)
{
    return normalizedProviderName(provider) == QLatin1String("ollama")
        ? ollamaApiServer()
        : openAiApiServer();
}

// Heuristic: real OpenAI-style API keys are well over 10 characters; shorter
// input is treated as "not configured yet" and rejected before any request.
inline bool isPlausibleApiKey(const QString& apiKey)
{
    return apiKey.trimmed().length() >= 10;
}

inline QString apiKeyErrorMessage()
{
    return QStringLiteral("Please provide the correct apikey");
}

} // namespace ProviderDefaults

#endif // PROVIDER_DEFAULTS_H

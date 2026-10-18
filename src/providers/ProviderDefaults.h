#ifndef PROVIDER_DEFAULTS_H
#define PROVIDER_DEFAULTS_H

#include <QString>

// Single source of truth for provider-related defaults shared by the
// settings store, the translation service and the model discovery service.
namespace ProviderDefaults {

// Canonical provider ids (the values stored in config.json and passed to
// QML). Use these instead of spelling the literals at every call site.
namespace ProviderIds {
inline QString openAi()
{
    return QStringLiteral("openai");
}

inline QString ollama()
{
    return QStringLiteral("ollama");
}
} // namespace ProviderIds

inline QString openAiApiServer()
{
    return QStringLiteral("https://api.openai.com");
}

inline QString ollamaApiServer()
{
    return QStringLiteral("http://localhost:11434");
}

// Default OpenAI chat model, used whenever the stored model name is empty.
inline QString openAiDefaultModel()
{
    return QStringLiteral("gpt-3.5-turbo");
}

inline QString normalizedProviderName(const QString& provider)
{
    return provider.trimmed().toLower() == QLatin1String("ollama")
        ? ProviderIds::ollama()
        : ProviderIds::openAi();
}

inline bool isOllamaProvider(const QString& provider)
{
    return normalizedProviderName(provider) == ProviderIds::ollama();
}

inline QString defaultApiServerForProvider(const QString& provider)
{
    return isOllamaProvider(provider) ? ollamaApiServer() : openAiApiServer();
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

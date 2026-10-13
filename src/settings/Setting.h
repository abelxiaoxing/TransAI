#ifndef SETTING_H
#define SETTING_H

#include <QObject>
#include <QString>

#include "providers/ProviderDefaults.h"
#include "PropertyMacros.h"

class Setting : public QObject
{
    Q_OBJECT
    Q_PROPERTY_AUTO(QString, shortCut);
    Q_PROPERTY_AUTO(QString, thinkingMode);
    Q_PROPERTY_AUTO(QString, openaiApiServer);
    Q_PROPERTY_AUTO(QString, openaiApiKey);
    Q_PROPERTY_AUTO(QString, openaiModel);
    Q_PROPERTY_AUTO(QString, ollamaApiServer);
    Q_PROPERTY_AUTO(QString, ollamaModel);

    // Active-provider view, computed on demand from the per-provider values
    // above. No members and no NOTIFY signals: QML only reads these
    // imperatively (translate-request assembly, settings reload), so a cached
    // copy would be redundant state that could drift from the source values.
    Q_PROPERTY(QString apiServer READ apiServer)
    Q_PROPERTY(QString apiKey READ apiKey)
    Q_PROPERTY(QString model READ model)
    Q_PROPERTY(QString provider READ provider NOTIFY providerChanged)

    // Provider defaults exposed to QML (settings page) so the UI never
    // hardcodes URLs or model names in parallel with the C++ side.
    Q_PROPERTY(QString defaultOpenAiApiServer READ defaultOpenAiApiServer CONSTANT)
    Q_PROPERTY(QString defaultOllamaApiServer READ defaultOllamaApiServer CONSTANT)
    Q_PROPERTY(QString defaultOpenAiModel READ defaultOpenAiModel CONSTANT)

public:
    explicit Setting(QObject* parent = nullptr);

    // Empty stored values fall back to the provider defaults, mirroring the
    // request-time fallback in TranslationService/ModelDiscoveryService.
    QString apiServer() const
    {
        if (_provider == QLatin1String("ollama")) {
            return _ollamaApiServer.trimmed().isEmpty() ? ProviderDefaults::ollamaApiServer() : _ollamaApiServer;
        }
        return _openaiApiServer.trimmed().isEmpty() ? ProviderDefaults::openAiApiServer() : _openaiApiServer;
    }
    // Ollama ignores API keys; the single stored key always belongs to the
    // OpenAI-compatible provider.
    QString apiKey() const { return _openaiApiKey; }
    QString model() const
    {
        if (_provider == QLatin1String("ollama")) {
            return _ollamaModel;
        }
        return _openaiModel.trimmed().isEmpty() ? ProviderDefaults::openAiDefaultModel() : _openaiModel;
    }
    QString provider() const { return _provider; }

    QString defaultOpenAiApiServer() const { return ProviderDefaults::openAiApiServer(); }
    QString defaultOllamaApiServer() const { return ProviderDefaults::ollamaApiServer(); }
    QString defaultOpenAiModel() const { return ProviderDefaults::openAiDefaultModel(); }

    // Shared plausibility check for API keys (mirrors the server-side check
    // in TranslationService/ModelDiscoveryService, for use before calls).
    Q_INVOKABLE bool hasPlausibleApiKey(const QString& apiKey) const
    {
        return ProviderDefaults::isPlausibleApiKey(apiKey);
    }

    Q_INVOKABLE bool loadConfig();
    Q_INVOKABLE void updateConfig();
    Q_INVOKABLE void applyProviderConfig(QString provider);

signals:
    void providerChanged();

private:
    QString _provider;
    QString m_configPath;
};

#endif // SETTING_H

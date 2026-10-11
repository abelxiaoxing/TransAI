#ifndef SETTING_H
#define SETTING_H

#include <QObject>
#include <QString>

#include "providers/ProviderDefaults.h"
#include "stdafx.h"

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

    // Active-provider view, derived from the per-provider values above.
    // Read-only on purpose: the single mutation path is applyProviderConfig()
    // plus the per-provider properties, so the two can never drift apart.
    Q_PROPERTY(QString apiServer READ apiServer NOTIFY apiServerChanged)
    Q_PROPERTY(QString apiKey READ apiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString model READ model NOTIFY modelChanged)
    Q_PROPERTY(QString provider READ provider NOTIFY providerChanged)

    // Provider defaults exposed to QML (settings page) so the UI never
    // hardcodes URLs or model names in parallel with the C++ side.
    Q_PROPERTY(QString defaultOpenAiApiServer READ defaultOpenAiApiServer CONSTANT)
    Q_PROPERTY(QString defaultOllamaApiServer READ defaultOllamaApiServer CONSTANT)
    Q_PROPERTY(QString defaultOpenAiModel READ defaultOpenAiModel CONSTANT)

public:
    explicit Setting(QObject* parent = nullptr);

    QString apiServer() const { return _apiServer; }
    QString apiKey() const { return _apiKey; }
    QString model() const { return _model; }
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
    void apiServerChanged();
    void apiKeyChanged();
    void modelChanged();
    void providerChanged();

private:
    void applyActiveProviderConfig();

    QString _apiServer;
    QString _apiKey;
    QString _model;
    QString _provider;
    QString m_configPath;
};

#endif // SETTING_H

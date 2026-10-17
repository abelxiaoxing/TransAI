#ifndef SETTING_H
#define SETTING_H

#include <QObject>
#include <QString>

#include "providers/ProviderDefaults.h"
#include "vision/VisionModelCapability.h"
#include "PropertyMacros.h"

class Setting : public QObject
{
    Q_OBJECT
    Q_PROPERTY_AUTO(QString, shortCut);
    // UI display language: "en" or "zh-CN" (already normalized; see
    // normalizedDisplayLanguage). QML's I18n singleton reads it and swaps
    // every bound string live, so no restart is needed after a change.
    Q_PROPERTY_AUTO(QString, displayLanguage);
    Q_PROPERTY_AUTO(QString, openaiApiServer);
    Q_PROPERTY_AUTO(QString, openaiApiKey);
    Q_PROPERTY_AUTO(QString, openaiModel);
    Q_PROPERTY_AUTO(QString, ollamaApiServer);
    Q_PROPERTY_AUTO(QString, ollamaModel);

    // Screenshot (multimodal) translation. Empty screenshotShortCut means
    // "not configured" (no hotkey is registered). screenshotTranslateMode is
    // "auto" | "vision" ("ocr" is reserved for a later phase). An empty
    // screenshotVisionModel means "follow the current translation model".
    Q_PROPERTY_AUTO(QString, screenshotShortCut);
    Q_PROPERTY_AUTO(QString, screenshotTranslateMode);
    Q_PROPERTY_AUTO(QString, screenshotVisionModel);
    Q_PROPERTY_AUTO(int, screenshotMaxImageWidth);
    Q_PROPERTY_AUTO(int, screenshotJpegQuality);

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

    // Maps any stored value onto the supported UI languages. "en" wins
    // verbatim; everything else — empty/unknown values included — resolves
    // zh* locales (stored or, when the value is empty, the system locale) to
    // "zh-CN" and all remaining locales to "en".
    static QString normalizedDisplayLanguage(const QString& language);

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

    // Heuristic (non-blocking) check used by the settings UI to warn when
    // the effective screenshot model probably cannot accept image input.
    Q_INVOKABLE bool isLikelyVisionModel(const QString& modelId) const
    {
        return VisionModelCapability::isLikelyVisionModel(modelId);
    }

    // Model actually used for screenshot translation: the explicit override
    // when set, otherwise the active provider's current model.
    Q_INVOKABLE QString effectiveScreenshotVisionModel() const
    {
        const QString overrideModel = _screenshotVisionModel.trimmed();
        return overrideModel.isEmpty() ? model() : overrideModel;
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

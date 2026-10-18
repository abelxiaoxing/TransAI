#ifndef SETTING_H
#define SETTING_H

#include <QObject>
#include <QString>

#include <memory>

#include "providers/ProviderDefaults.h"
#include "vision/VisionModelCapability.h"
#include "PropertyMacros.h"

class SecretStore;

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
    // "auto" | "vision" | "ocr" (OCR falls back to vision when no backend is
    // available; see AppView.effectiveScreenshotMode()). An empty
    // screenshotVisionModel means "follow the current translation model".
    Q_PROPERTY_AUTO(QString, screenshotShortCut);
    Q_PROPERTY_AUTO(QString, screenshotTranslateMode);
    Q_PROPERTY_AUTO(QString, screenshotVisionModel);
    Q_PROPERTY_AUTO(int, screenshotMaxImageWidth);
    Q_PROPERTY_AUTO(int, screenshotJpegQuality);

    // Main-view preference. Empty targetLanguage means "not set" (QML falls
    // back to the first entry of its language list).
    Q_PROPERTY_AUTO(QString, targetLanguage);

    // Main-window geometry. 0 means "not set" (QML falls back to its
    // defaults); loaded values are clamped to the window's min/max bounds.
    // Position (x/y) is deliberately not persisted: Wayland clients cannot
    // position themselves, and the centered default + ensureWindowVisible()
    // already keeps the window on screen.
    Q_PROPERTY_AUTO(int, windowWidth);
    Q_PROPERTY_AUTO(int, windowHeight);

    // True when the API key currently lives in the OS keychain (and therefore
    // not in config.json). Read-only UI hint; updated by loadConfig() and
    // updateConfig().
    Q_PROPERTY(bool apiKeyStoredSecurely READ apiKeyStoredSecurely NOTIFY apiKeyStoredSecurelyChanged)

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

    // Window geometry: single source of truth shared with main.qml (the window
    // sizes itself from these and loadConfig() clamps persisted values to the
    // same bounds).
    Q_PROPERTY(int defaultWindowWidth READ defaultWindowWidth CONSTANT)
    Q_PROPERTY(int defaultWindowHeight READ defaultWindowHeight CONSTANT)
    Q_PROPERTY(int minWindowWidth READ minWindowWidth CONSTANT)
    Q_PROPERTY(int minWindowHeight READ minWindowHeight CONSTANT)
    Q_PROPERTY(int maxWindowWidth READ maxWindowWidth CONSTANT)
    Q_PROPERTY(int maxWindowHeight READ maxWindowHeight CONSTANT)

public:
    static constexpr int kDefaultWindowWidth = 460;
    static constexpr int kDefaultWindowHeight = 700;
    static constexpr int kMinWindowWidth = 400;
    static constexpr int kMaxWindowWidth = 3840;
    static constexpr int kMinWindowHeight = 500;
    static constexpr int kMaxWindowHeight = 2160;

    explicit Setting(QObject* parent = nullptr);
    // Test seam: inject a fake SecretStore to exercise the keychain
    // read/migration/fallback logic deterministically.
    Setting(QObject* parent, std::unique_ptr<SecretStore> secretStore);
    // Out-of-line: unique_ptr<SecretStore> needs the complete type.
    ~Setting() override;

    // Maps any stored value onto the supported UI languages. "en" wins
    // verbatim; everything else — empty/unknown values included — resolves
    // zh* locales (stored or, when the value is empty, the system locale) to
    // "zh-CN" and all remaining locales to "en".
    static QString normalizedDisplayLanguage(const QString& language);

    // Empty stored values fall back to the provider defaults, mirroring the
    // request-time fallback in TranslationService/ModelDiscoveryService.
    QString apiServer() const
    {
        if (ProviderDefaults::isOllamaProvider(_provider)) {
            return _ollamaApiServer.trimmed().isEmpty() ? ProviderDefaults::ollamaApiServer() : _ollamaApiServer;
        }
        return _openaiApiServer.trimmed().isEmpty() ? ProviderDefaults::openAiApiServer() : _openaiApiServer;
    }
    // Ollama ignores API keys; the single stored key always belongs to the
    // OpenAI-compatible provider. Trimmed so a pasted key with stray
    // whitespace is never sent (or stored) verbatim.
    QString apiKey() const { return _openaiApiKey.trimmed(); }
    QString model() const
    {
        if (ProviderDefaults::isOllamaProvider(_provider)) {
            return _ollamaModel;
        }
        return _openaiModel.trimmed().isEmpty() ? ProviderDefaults::openAiDefaultModel() : _openaiModel;
    }
    QString provider() const { return _provider; }

    bool apiKeyStoredSecurely() const { return m_apiKeyStoredSecurely; }

    int defaultWindowWidth() const { return kDefaultWindowWidth; }
    int defaultWindowHeight() const { return kDefaultWindowHeight; }
    int minWindowWidth() const { return kMinWindowWidth; }
    int minWindowHeight() const { return kMinWindowHeight; }
    int maxWindowWidth() const { return kMaxWindowWidth; }
    int maxWindowHeight() const { return kMaxWindowHeight; }

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
    void apiKeyStoredSecurelyChanged();

private:
    QString _provider;
    QString m_configPath;
    std::unique_ptr<SecretStore> m_secretStore;
    void setApiKeyStoredSecurely(bool storedSecurely);

    // Set by loadConfig() when a plaintext API key was migrated into the
    // secret store; the constructor then re-saves to drop the plaintext.
    bool m_apiKeyMigratedToStore = false;
    // Last API key value actually written to (or read from) the secret store.
    // updateConfig() skips the keychain round-trip when the key is unchanged:
    // it runs on every settings edit, and a keychain write per keystroke is
    // both slow and (with some backends) a blocking prompt.
    QString m_lastStoredApiKey;
    bool m_apiKeyStoredSecurely = false;
};

#endif // SETTING_H

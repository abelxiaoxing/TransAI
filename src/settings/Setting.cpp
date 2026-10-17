#include "settings/Setting.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDebug>

#include "providers/ProviderDefaults.h"
#include "settings/SecretStore.h"

namespace {
// Secret-store entry id for the OpenAI-compatible provider key (Ollama has
// no key). Config marker key records that the key lives in the secret store.
const char kSecretOpenAiApiKeyId[] = "openaiApiKey";
const char kOpenAIApiKeyInStoreKey[] = "openaiApiKeyInSecretStore";
}

namespace {
const char kConfigFileName[] = "config.json";
// Legacy top-level keys from the pre-per-provider era. They are only READ
// (migrated) in loadConfig(); updateConfig() no longer writes them, so a
// downgrade to a pre-migration binary sees the last state frozen on disk.
const char kApiServerKey[] = "apiServer";
const char kApiKeyKey[] = "apiKey";
const char kModelKey[] = "model";
const char kOpenAIApiServerKey[] = "openaiApiServer";
const char kOpenAIModelKey[] = "openaiModel";
const char kOpenAIApiKeyKey[] = "openaiApiKey";
const char kOllamaApiServerKey[] = "ollamaApiServer";
const char kOllamaModelKey[] = "ollamaModel";
const char kShortCutKey[] = "shortCut";
const char kProviderKey[] = "provider";
const char kDisplayLanguageKey[] = "displayLanguage";
const char kScreenshotShortCutKey[] = "screenshotShortCut";
const char kScreenshotTranslateModeKey[] = "screenshotTranslateMode";
const char kScreenshotVisionModelKey[] = "screenshotVisionModel";
const char kScreenshotMaxImageWidthKey[] = "screenshotMaxImageWidth";
const char kScreenshotJpegQualityKey[] = "screenshotJpegQuality";
const char kTargetLanguageKey[] = "targetLanguage";
const char kTranslationModeKey[] = "translationMode";
const char kWindowWidthKey[] = "windowWidth";
const char kWindowHeightKey[] = "windowHeight";
const char kConfigVersionKey[] = "configVersion";
constexpr int kConfigVersion = 2;
constexpr int kDefaultMaxImageWidth = 1600;
constexpr int kDefaultJpegQuality = 85;
// Window-geometry bounds, aligned with main.qml's minimumWidth/Height.
constexpr int kMinWindowWidth = 400;
constexpr int kMaxWindowWidth = 3840;
constexpr int kMinWindowHeight = 500;
constexpr int kMaxWindowHeight = 2160;

QStringList existingConfigCandidates()
{
    QStringList candidates;
    const QStringList appConfigDirs = QStandardPaths::standardLocations(QStandardPaths::AppConfigLocation);
    for (const QString& baseDir : appConfigDirs) {
        if (!baseDir.isEmpty()) {
            candidates << QDir(baseDir).filePath(kConfigFileName);
        }
    }
    return candidates;
}

QString resolveConfigPath()
{
    for (const QString& path : existingConfigCandidates()) {
        if (!path.isEmpty() && QFileInfo(path).exists() && QFileInfo(path).isFile()) {
            return path;
        }
    }

    const QString writableDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (writableDir.isEmpty()) {
        return QString();
    }
    return QDir(writableDir).filePath(kConfigFileName);
}

void backupIfInvalidConfig(const QString& path)
{
    if (!QFileInfo(path).exists() || path.isEmpty()) {
        return;
    }
    const QString backupPath = path + ".invalid.bak";
    if (QFileInfo(backupPath).exists()) {
        return;
    }
    QFile::copy(path, backupPath);
}

bool ensureConfigDirExists(const QString& configPath)
{
    const QFileInfo info(configPath);
    return info.dir().exists() || info.dir().mkpath(".");
}
}

Setting::Setting(QObject* parent)
    : Setting(parent, createPlatformSecretStore())
{
}

Setting::~Setting() = default;

Setting::Setting(QObject* parent, std::unique_ptr<SecretStore> secretStore)
    : QObject(parent)
    , m_secretStore(std::move(secretStore))
{
    // QString members default to empty; only the non-empty defaults need to
    // be spelled out here.
    _openaiApiServer = ProviderDefaults::openAiApiServer();
    _openaiModel = ProviderDefaults::openAiDefaultModel();
    _ollamaApiServer = ProviderDefaults::ollamaApiServer();
    _provider = "openai";
    // Screenshot-translation defaults: vision-first, follow current model.
    _screenshotTranslateMode = "auto";
    _screenshotMaxImageWidth = kDefaultMaxImageWidth;
    _screenshotJpegQuality = kDefaultJpegQuality;
    // Main-view preferences: follow the QML defaults (first language entry,
    // Translate mode). QString members default to empty; the int member of
    // Q_PROPERTY_AUTO is uninitialized unless spelled out here.
    _translationMode = 0;
    // Window geometry: 0 = not set (QML falls back to its default geometry).
    _windowWidth = 0;
    _windowHeight = 0;
    // First launch (config without a displayLanguage key) follows the system
    // UI language; normalizedDisplayLanguage handles that fallback.
    _displayLanguage = normalizedDisplayLanguage(QString());

    m_configPath = resolveConfigPath();
    if (m_configPath.isEmpty()) {
        qWarning() << "No writable config location found";
        return;
    }

    if (!ensureConfigDirExists(m_configPath)) {
        qWarning() << "Failed to create config directory:" << m_configPath;
        return;
    }

    QFile configFile(m_configPath);
    if (configFile.exists()) {
        if (!loadConfig()) {
            backupIfInvalidConfig(m_configPath);
            qWarning() << "Invalid config file is kept and backed up to .invalid.bak:" << m_configPath;
        }
    } else {
        updateConfig();
    }
    // A migrated API key left its plaintext in the just-loaded config;
    // re-save now so the file no longer carries it.
    if (m_apiKeyMigratedToStore) {
        updateConfig();
        m_apiKeyMigratedToStore = false;
    }
}

QString Setting::normalizedDisplayLanguage(const QString& language)
{
    if (language.compare(QLatin1String("en"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("en");
    }
    // Anything else — including empty or unknown values — resolves zh* locales
    // to the single supported Chinese variant and every other locale to
    // English. An empty value probes the system locale, so a first launch
    // without the key follows the system UI language.
    const QString probe = language.trimmed().isEmpty() ? QLocale::system().name() : language;
    if (QLocale(probe).language() == QLocale::Chinese) {
        return QStringLiteral("zh-CN");
    }
    return QStringLiteral("en");
}

bool Setting::loadConfig()
{
    QFile file(m_configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file for read:" << m_configPath;
        return false;
    }

    QJsonParseError parseError{};
    QByteArray rawData = file.readAll();
    file.close();
    if (rawData.startsWith("\xEF\xBB\xBF")) {
        rawData.remove(0, 3);
    }

    const QJsonDocument doc = QJsonDocument::fromJson(rawData.trimmed(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "Invalid config JSON:" << parseError.errorString() << m_configPath;
        return false;
    }

    const QJsonObject obj = doc.object();
    const QString legacyApiServer = obj.value(kApiServerKey).toString();
    const QString legacyApiKey = obj.value(kApiKeyKey).toString();
    const QString legacyModel = obj.value(kModelKey).toString();

    _provider = ProviderDefaults::normalizedProviderName(obj.value(kProviderKey).toString("openai"));

    // "Key present but empty" is preserved as empty (the derived active view
    // falls back to the default); "key absent" migrates from the legacy
    // top-level keys or falls back to the default.
    const QString storedOpenAiServer = obj.value(kOpenAIApiServerKey).toString();
    if (storedOpenAiServer.trimmed().isEmpty() && !obj.contains(kOpenAIApiServerKey)) {
        _openaiApiServer = _provider == "openai" && !legacyApiServer.trimmed().isEmpty()
            ? legacyApiServer.trimmed()
            : ProviderDefaults::openAiApiServer();
    } else {
        _openaiApiServer = storedOpenAiServer;
    }

    _openaiApiKey = obj.value(kOpenAIApiKeyKey).toString();
    if (_openaiApiKey.trimmed().isEmpty() && !obj.contains(kOpenAIApiKeyKey)) {
        _openaiApiKey = legacyApiKey;
    }

    // Secret store (OS keychain) integration: the store wins over config
    // plaintext; a plaintext-only key is migrated into the store once —
    // verified by read-back — and cleared from the file on the re-save the
    // constructor performs (m_apiKeyMigratedToStore). On any store failure
    // the plaintext stays untouched (previous behavior).
    if (m_secretStore && m_secretStore->isAvailable()) {
        const QString storeId = QString::fromLatin1(kSecretOpenAiApiKeyId);
        const QString storedKey = m_secretStore->read(storeId);
        if (!storedKey.isEmpty()) {
            _openaiApiKey = storedKey;
        } else if (!_openaiApiKey.trimmed().isEmpty()) {
            if (m_secretStore->write(storeId, _openaiApiKey)
                && m_secretStore->read(storeId) == _openaiApiKey) {
                m_apiKeyMigratedToStore = true;
            } else {
                qWarning() << "API key migration to the secret store failed; keeping config.json plaintext";
            }
        }
    }

    _openaiModel = obj.value(kOpenAIModelKey).toString();
    if (_openaiModel.trimmed().isEmpty() && !obj.contains(kOpenAIModelKey)) {
        _openaiModel = _provider == "openai" && !legacyModel.trimmed().isEmpty()
            ? legacyModel.trimmed()
            : ProviderDefaults::openAiDefaultModel();
    }

    _ollamaApiServer = obj.value(kOllamaApiServerKey).toString();
    if (_ollamaApiServer.trimmed().isEmpty()) {
        _ollamaApiServer = ProviderDefaults::ollamaApiServer();
    }
    _ollamaModel = obj.value(kOllamaModelKey).toString();
    _shortCut = obj.value(kShortCutKey).toString();

    // Screenshot translation keys (absent = defaults; older configs keep
    // working unchanged).
    _screenshotShortCut = obj.value(kScreenshotShortCutKey).toString();
    const QString mode = obj.value(kScreenshotTranslateModeKey).toString().trimmed().toLower();
    _screenshotTranslateMode = (mode == QLatin1String("vision") || mode == QLatin1String("ocr"))
        ? mode
        : QStringLiteral("auto");
    _screenshotVisionModel = obj.value(kScreenshotVisionModelKey).toString();
    _screenshotMaxImageWidth = obj.value(kScreenshotMaxImageWidthKey).toInt(kDefaultMaxImageWidth);
    if (_screenshotMaxImageWidth <= 0) {
        _screenshotMaxImageWidth = kDefaultMaxImageWidth;
    }
    _screenshotJpegQuality = obj.value(kScreenshotJpegQualityKey).toInt(kDefaultJpegQuality);
    if (_screenshotJpegQuality <= 0 || _screenshotJpegQuality > 100) {
        _screenshotJpegQuality = kDefaultJpegQuality;
    }

    // Main-view preferences (absent = defaults). translationMode mirrors the
    // TranslationMode enum (0..2); out-of-range values fall back to 0.
    _targetLanguage = obj.value(kTargetLanguageKey).toString();
    _translationMode = obj.value(kTranslationModeKey).toInt(0);
    if (_translationMode < 0 || _translationMode > 2) {
        _translationMode = 0;
    }

    // Window geometry (absent/invalid = 0, QML falls back to its defaults).
    const int windowWidth = obj.value(kWindowWidthKey).toInt(0);
    _windowWidth = (windowWidth >= kMinWindowWidth && windowWidth <= kMaxWindowWidth) ? windowWidth : 0;
    const int windowHeight = obj.value(kWindowHeightKey).toInt(0);
    _windowHeight = (windowHeight >= kMinWindowHeight && windowHeight <= kMaxWindowHeight) ? windowHeight : 0;

    const QString parsedDisplayLanguage = normalizedDisplayLanguage(obj.value(kDisplayLanguageKey).toString());
    if (_displayLanguage != parsedDisplayLanguage) {
        _displayLanguage = parsedDisplayLanguage;
        emit displayLanguageChanged();
    }

    return true;
}

void Setting::applyProviderConfig(QString provider)
{
    const QString normalized = ProviderDefaults::normalizedProviderName(provider);
    if (_provider == normalized) {
        return;
    }
    _provider = normalized;
    emit providerChanged();
}

void Setting::updateConfig()
{
    if (m_configPath.isEmpty()) {
        return;
    }

    // API key persistence: prefer the secret store (OS keychain); the
    // plaintext copy is dropped only after a write verified by read-back.
    // On any failure the key stays in config.json exactly as before.
    QString apiKeyForConfig = _openaiApiKey;
    bool keyInStore = false;
    if (m_secretStore && m_secretStore->isAvailable()) {
        const QString storeId = QString::fromLatin1(kSecretOpenAiApiKeyId);
        if (_openaiApiKey.isEmpty()) {
            keyInStore = m_secretStore->remove(storeId);
        } else if (m_secretStore->write(storeId, _openaiApiKey)
                   && m_secretStore->read(storeId) == _openaiApiKey) {
            keyInStore = true;
        }
        if (keyInStore) {
            apiKeyForConfig.clear();
        } else {
            qWarning() << "Secret store write failed; API key stays in config.json";
        }
    }

    QJsonObject obj{
        {kConfigVersionKey, kConfigVersion},
        {kShortCutKey, _shortCut},
        {kProviderKey, _provider},
        {kDisplayLanguageKey, _displayLanguage},
        {kOpenAIApiServerKey, _openaiApiServer},
        {kOpenAIApiKeyKey, apiKeyForConfig},
        {kOpenAIApiKeyInStoreKey, keyInStore},
        {kOpenAIModelKey, _openaiModel},
        {kOllamaApiServerKey, _ollamaApiServer},
        {kOllamaModelKey, _ollamaModel},
        {kScreenshotShortCutKey, _screenshotShortCut},
        {kScreenshotTranslateModeKey, _screenshotTranslateMode},
        {kScreenshotVisionModelKey, _screenshotVisionModel},
        {kScreenshotMaxImageWidthKey, _screenshotMaxImageWidth},
        {kScreenshotJpegQualityKey, _screenshotJpegQuality},
        {kTargetLanguageKey, _targetLanguage},
        {kTranslationModeKey, _translationMode},
        {kWindowWidthKey, _windowWidth},
        {kWindowHeightKey, _windowHeight},
    };

    QSaveFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Failed to open config file for write:" << m_configPath;
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        qWarning() << "Failed to commit config file:" << file.errorString();
        return;
    }
#ifdef Q_OS_UNIX
    // The config contains the API key in plain text; keep it private.
    QFile::setPermissions(m_configPath, QFile::ReadOwner | QFile::WriteOwner);
#endif
}

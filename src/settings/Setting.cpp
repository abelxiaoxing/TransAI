#include "settings/Setting.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDebug>

#include "providers/ProviderDefaults.h"
#include "providers/ThinkingPolicy.h"

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
const char kThinkingModeKey[] = "thinkingMode";
const char kConfigVersionKey[] = "configVersion";
constexpr int kConfigVersion = 2;

QStringList existingConfigCandidates()
{
    QStringList candidates;
    const QStringList appConfigDirs = QStandardPaths::standardLocations(QStandardPaths::AppConfigLocation);
    for (const QString& baseDir : appConfigDirs) {
        if (!baseDir.isEmpty()) {
            candidates << QDir(baseDir).filePath(kConfigFileName);
        }
    }

    const QString located = QStandardPaths::locate(QStandardPaths::AppConfigLocation, kConfigFileName, QStandardPaths::LocateFile);
    if (!located.isEmpty()) {
        candidates.prepend(located);
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
    : QObject(parent)
{
    _openaiApiServer = ProviderDefaults::openAiApiServer();
    _openaiApiKey = "";
    _openaiModel = "gpt-3.5-turbo";
    _ollamaApiServer = ProviderDefaults::ollamaApiServer();
    _ollamaModel = "";
    _shortCut = "";
    _provider = "openai";
    _thinkingMode = "auto";
    applyActiveProviderConfig();

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

    _provider = obj.value(kProviderKey).toString("openai");
    if (_provider != "openai" && _provider != "ollama") {
        _provider = "openai";
    }

    _openaiApiServer = obj.value(kOpenAIApiServerKey).toString();
    if (_openaiApiServer.trimmed().isEmpty() && obj.contains(kOpenAIApiServerKey)) {
        _openaiApiServer = "";
    } else if (_openaiApiServer.trimmed().isEmpty()) {
        _openaiApiServer = _provider == "openai"
            ? (legacyApiServer.trimmed().isEmpty() ? ProviderDefaults::openAiApiServer() : legacyApiServer.trimmed())
            : ProviderDefaults::openAiApiServer();
    }

    _openaiApiKey = obj.value(kOpenAIApiKeyKey).toString();
    if (_openaiApiKey.trimmed().isEmpty() && !obj.contains(kOpenAIApiKeyKey)) {
        _openaiApiKey = legacyApiKey;
    }

    _openaiModel = obj.value(kOpenAIModelKey).toString();
    if (_openaiModel.trimmed().isEmpty() && !obj.contains(kOpenAIModelKey)) {
        _openaiModel = _provider == "openai"
            ? (legacyModel.trimmed().isEmpty() ? "gpt-3.5-turbo" : legacyModel.trimmed())
            : "gpt-3.5-turbo";
    }

    _ollamaApiServer = obj.value(kOllamaApiServerKey).toString();
    if (_ollamaApiServer.trimmed().isEmpty()) {
        _ollamaApiServer = ProviderDefaults::ollamaApiServer();
    }
    _ollamaModel = obj.value(kOllamaModelKey).toString();
    _shortCut = obj.value(kShortCutKey).toString();

    const QString parsedThinkingMode =
        ThinkingPolicyBuilder::normalizeThinkingMode(obj.value(kThinkingModeKey).toString());
    if (_thinkingMode != parsedThinkingMode) {
        _thinkingMode = parsedThinkingMode;
        emit thinkingModeChanged();
    }

    applyActiveProviderConfig();
    return true;
}

void Setting::applyProviderConfig(QString provider)
{
    const QString normalized = ProviderDefaults::normalizedProviderName(provider);
    if (_provider == normalized) {
        applyActiveProviderConfig();
        return;
    }
    _provider = normalized;
    emit providerChanged();
    applyActiveProviderConfig();
}

void Setting::applyActiveProviderConfig()
{
    const QString nextApiServer = _provider == "ollama"
        ? (_ollamaApiServer.trimmed().isEmpty() ? ProviderDefaults::ollamaApiServer() : _ollamaApiServer)
        : (_openaiApiServer.trimmed().isEmpty() ? ProviderDefaults::openAiApiServer() : _openaiApiServer);
    const QString nextApiKey = _openaiApiKey;
    const QString nextModel = _provider == "ollama"
        ? _ollamaModel
        : (_openaiModel.trimmed().isEmpty() ? "gpt-3.5-turbo" : _openaiModel);

    if (_apiServer != nextApiServer) {
        _apiServer = nextApiServer;
        emit apiServerChanged();
    }
    if (_apiKey != nextApiKey) {
        _apiKey = nextApiKey;
        emit apiKeyChanged();
    }
    if (_model != nextModel) {
        _model = nextModel;
        emit modelChanged();
    }
}

void Setting::updateConfig()
{
    if (m_configPath.isEmpty()) {
        return;
    }

    QJsonObject obj{
        {kConfigVersionKey, kConfigVersion},
        {kShortCutKey, _shortCut},
        {kProviderKey, _provider},
        {kThinkingModeKey, _thinkingMode},
        {kOpenAIApiServerKey, _openaiApiServer},
        {kOpenAIApiKeyKey, _openaiApiKey},
        {kOpenAIModelKey, _openaiModel},
        {kOllamaApiServerKey, _ollamaApiServer},
        {kOllamaModelKey, _ollamaModel},
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

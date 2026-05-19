#include "controller.h"

#include <algorithm>
#include <QJsonParseError>
#include <QSaveFile>

using namespace std;

namespace {
const char kConfigFileName[] = "config.json";
const char kApiServerKey[] = "apiServer";
const char kApiKeyKey[] = "apiKey";
const char kModelKey[] = "model";
const char kShortCutKey[] = "shortCut";
const char kProviderKey[] = "provider";
const char kDefaultApiServer[] = "https://api.openai.com";
const char kDefaultOllamaApiServer[] = "http://localhost:11434";

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

Setting::Setting(QObject * parent): QObject{parent}
{
    _apiServer = kDefaultApiServer;
    _apiKey = "";
    _model = "gpt-3.5-turbo";
    _shortCut = "";
    _provider = "openai";
    _configPath = resolveConfigPath();
    if (_configPath.isEmpty()) {
        assert("no writeable location found");
        return;
    }

    if (!ensureConfigDirExists(_configPath)) {
        qWarning() << "Failed to create config directory:" << _configPath;
        return;
    }

    QFile configFile(_configPath);
    if (configFile.exists()) {
        if (!loadConfig()) {
            backupIfInvalidConfig(_configPath);
            qWarning() << "Invalid config file is kept and backed up to .invalid.bak:" << _configPath;
        }
    } else {
        updateConfig();
    }
}

bool Setting::loadConfig()
{
    QFile file(_configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file for read:" << _configPath;
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
        qWarning() << "Invalid config JSON:" << parseError.errorString() << _configPath;
        return false;
    }

   QJsonObject obj = doc.object();
   _apiKey = obj.value(kApiKeyKey).toString();
   _model = obj.value(kModelKey).toString();
   _apiServer = obj.value(kApiServerKey).toString();
   _shortCut = obj.value(kShortCutKey).toString();
   _provider = obj.value(kProviderKey).toString("openai");
   if (_model.trimmed().isEmpty()) {
       _model = "gpt-3.5-turbo";
   }
   if (_provider == "ollama") {
       _apiServer = kDefaultOllamaApiServer;
   } else if (_apiServer.trimmed().isEmpty()) {
       _apiServer = kDefaultApiServer;
   }
   return true;
}

void Setting::updateConfig()
{
    if (_configPath.isEmpty()) {
        return;
    }

    QJsonObject obj{
        {kApiKeyKey, _apiKey},
        {kModelKey, _model},
        {kApiServerKey, _apiServer},
        {kShortCutKey, _shortCut},
        {kProviderKey, _provider},
    };
    const QJsonDocument doc(obj);
    QSaveFile file(_configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Failed to open config file for write:" << _configPath;
        return;
    }
    file.write(doc.toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        qWarning() << "Failed to commit config file:" << file.errorString();
    }
}


Controller::Controller(QObject *parent)
    : QObject{parent}
{
    _responseData = "";
    _responseError = "";
    _transToLang = "";
    _isRequesting = false;
    _apiServer = "";
    _apiKey = "";
    _model = "";
    _provider = "openai";
    _availableModels = QStringList();
    _isDetectingModels = false;
    _modelDetectError = "";
    networkManager = new QNetworkAccessManager(this);
    reply = nullptr;
    modelReply = nullptr;
    _themeController = new ThemeController(this);

}

QJsonObject Controller::createMessage(const QString& role,const QString& content){
    QJsonObject message;
    message.insert("role",role);
    message.insert("content",content);
    return message;
}


std::tuple<QString, bool> Controller::_getContent(QString &str)
{
    QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
    if (!doc.isNull()) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
             QString text = "";
            if(obj.contains("error")){
                text = obj.value("error").toObject().value("message").toString();
                return std::make_tuple(text, false);
            }else{
                text = obj.value("choices").toArray().at(0).toObject().value("delta").toObject().value("content").toString();
                return std::make_tuple(text, true);
            }
        }
    }
    return std::make_tuple("", false);
}

std::tuple<QString, bool> Controller::_parseResponse(QByteArray &ba)
{
    QString data;
    bool error = false;
    QStringList lines = QString::fromUtf8(ba).split("data:");
    for (const QString &line : lines) {
        QString eventData = line.trimmed();;
//        qDebug() <<eventData;
        QString text;
        bool haveError;
        std::tie(text, haveError) = _getContent(eventData);
        data += text;
        error |= haveError;
    }
    return std::make_tuple(data, error);

}

QUrl Controller::_buildModelListUrl(const QString& apiServer, const QString& provider)
{
    if (provider == "ollama") {
        QUrl url("http://localhost:11434");
        if (url.path().contains("/v1/chat/completions")) {
            url.setPath(url.path().replace("/v1/chat/completions", "/api/tags"));
        } else if (url.path().contains("/api/chat")) {
            url.setPath(url.path().replace("/api/chat", "/api/tags"));
        } else if (!url.path().endsWith("/api/tags")) {
            url.setPath("/api/tags");
        }
        url.setQuery(QString());
        return url;
    }

    QUrl url(apiServer.trimmed().isEmpty() ? "https://api.openai.com" : apiServer.trimmed());
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

QStringList Controller::_parseModelList(const QByteArray& data, const QString& provider, QString* errorMessage)
{
    QStringList models;
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = parseError.errorString();
        }
        return models;
    }

    QJsonObject obj = doc.object();
    if (obj.contains("error")) {
        QJsonValue errorValue = obj.value("error");
        if (errorMessage) {
            if (errorValue.isObject()) {
                *errorMessage = errorValue.toObject().value("message").toString("Failed to detect models");
            } else {
                *errorMessage = errorValue.toString("Failed to detect models");
            }
        }
        return models;
    }

    QJsonArray modelArray = provider == "ollama" ? obj.value("models").toArray() : obj.value("data").toArray();
    for (const QJsonValue& value : modelArray) {
        QJsonObject modelObj = value.toObject();
        QString id = provider == "ollama" ? modelObj.value("name").toString() : modelObj.value("id").toString();
        if (!id.isEmpty() && !models.contains(id)) {
            models.append(id);
        }
    }

    std::sort(models.begin(), models.end(), [](const QString& a, const QString& b) {
        return QString::localeAwareCompare(a, b) < 0;
    });
    return models;
}


void Controller::streamReceived()
{

    QByteArray response = reply->readAll();
    QString text;
    bool haveError;
    std::tie(text, haveError) = _parseResponse(response);
    _data += text;
    responseData(_data);

}


void Controller::detectModels(QString apiServer, QString apiKey, QString provider)
{
    if (modelReply) {
        disconnect(modelReply, nullptr, this, nullptr);
        modelReply->abort();
        modelReply->deleteLater();
        modelReply = nullptr;
    }

    QString normalizedProvider = provider == "ollama" ? "ollama" : "openai";
    if (normalizedProvider != "ollama" && apiKey.trimmed().length() < 10) {
        modelDetectError("Please provide the correct apikey");
        availableModels(QStringList());
        return;
    }

    modelDetectError("");
    isDetectingModels(true);

    QNetworkRequest request(_buildModelListUrl(apiServer, normalizedProvider));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    if (normalizedProvider != "ollama") {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey.trimmed()).toUtf8());
    }

    modelReply = networkManager->get(request);
    QNetworkReply* currentReply = modelReply;
    connect(currentReply, &QNetworkReply::finished, this, [this, currentReply, normalizedProvider]() {
        if (modelReply != currentReply) {
            currentReply->deleteLater();
            return;
        }
        modelReply = nullptr;
        isDetectingModels(false);

        QByteArray response = currentReply->readAll();
        if (currentReply->error() == QNetworkReply::NoError) {
            QString parseError;
            QStringList models = _parseModelList(response, normalizedProvider, &parseError);
            availableModels(models);
            modelDetectError(models.isEmpty() ? (parseError.isEmpty() ? "No models found" : parseError) : "");
        } else if (currentReply->error() != QNetworkReply::OperationCanceledError) {
            QString parseError;
            QStringList models = _parseModelList(response, normalizedProvider, &parseError);
            availableModels(models);
            modelDetectError(parseError.isEmpty() ? currentReply->errorString() : parseError);
        }

        currentReply->deleteLater();
    });
}

void Controller::sendMessage(QString str, int mode)
{
    if(_apiServer.trimmed().length() == 0){
        if(_provider == "ollama"){
            _apiServer = "http://localhost:11434";
        }else{
            _apiServer = "https://api.openai.com";
        }
    }

    // Ollama doesn't require API key
    if(_provider != "ollama" && _apiKey.length() < 10){
        responseError("Please provide the correct apikey");
        return;
    }
    QUrl apiUrl(_provider == "ollama" ? "http://localhost:11434/v1/chat/completions" : _apiServer);
      QNetworkRequest request(apiUrl);
      request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
      if(_provider != "ollama"){
          request.setRawHeader("Authorization", QString::fromStdString("Bearer %1").arg(_apiKey.trimmed()).toUtf8());
      }
      request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork); // Events shouldn't be cached

      QJsonObject requestData;
      QJsonArray messages;
      requestData.insert("model", _model);
      requestData.insert("stream", true);
//      qDebug() << _transToLang;
      QString systemcmd;
      if(mode == 0){
        systemcmd = QString::fromStdString("Translate the text to %1, which is delimited with triple backticks. Only return the translate result, don’t interpret it, don't return the delimited character.").arg(_transToLang);
        messages.append(createMessage("system",systemcmd));
        messages.append(createMessage("user", "translate text:'''" + str + "'''" ));
      }else if(mode == 1){
          systemcmd = QString::fromStdString("Translate anything that I say to %1. When the text contains only one single word, please provide the original form (if applicable), \
  the language of the word, the corresponding phonetic transcription (if applicable), \
  all meanings (including parts of speech), and at least three bilingual examples. Please strictly follow the format below:\
                                             <Original Text> \n \
                                             [<Language>] · / <Phonetic Transcription> \n \
                                             [<Part of Speech Abbreviation>] <Chinese Meaning>] \n \
                                             Examples: \n\
                                             <Number><Example>(Example Translation).The content in this format must be %1 either").arg(_transToLang);
          messages.append(createMessage("system",systemcmd));
          messages.append(createMessage("user","\"" + str + "\""));
      }else{
        systemcmd = QString::fromStdString("I want you to strictly correct my grammar mistakes, typos, and factual errors.Only correct sentence in the brackets.").arg(_transToLang);
        messages.append(createMessage("system",systemcmd));
        messages.append(createMessage("user", " The sentence is: ["+ str + "]"));
      }



      requestData.insert("messages", messages);
      QJsonDocument requestDoc(requestData);
      QByteArray requestDataBytes = requestDoc.toJson();
//      qDebug() << requestDataBytes;

      _data = "";
      responseData(_data);
      isRequesting(true);
      reply = networkManager->post(request, requestDataBytes);
      connect(reply, SIGNAL(readyRead()), this, SLOT(streamReceived()));


      connect(reply, &QNetworkReply::finished,this, [=]() {
          isRequesting(false);
          qDebug() << "finished";
          QByteArray response = reply->readAll();
          QString text;
          bool haveError;
          std::tie(text, haveError) = _parseResponse(response);
          _data += text;
          responseData(_data);
          if (reply->error() == QNetworkReply::NoError) {
              responseError("");
          } else {
              if(reply->error() > 0 && reply->error() < 100){
                  if(reply->error() != QNetworkReply::OperationCanceledError){
                     responseError("network error");
                  }

              }else if(reply->error() > 100 && reply->error() < 200){
                  responseError("proxy error");
              }else if(reply->error() > 200 && reply->error() < 300){
                  responseError("content error");
              }else if(reply->error() > 300 && reply->error() < 400){
                  responseError("protocol error");
              }else if(reply->error() > 400 && reply->error() < 500){
                  responseError("server error");
              }
              qDebug() << reply->error() ;
              qDebug() << "网络错误："+  reply->errorString();
          }
          reply->deleteLater();
          reply = nullptr;
      });
}


void Controller::abort()
{
    try {
        if(reply)
        reply->abort();
    } catch (...) {
    }
}

// ThemeController Implementation
ThemeController::ThemeController(QObject *parent) : QObject(parent)
{
    _themeManager = new ThemeManager(this);
    _currentTheme = _themeManager->currentTheme();
}

ThemeManager::Theme ThemeController::currentTheme() const
{
    return _currentTheme;
}

void ThemeController::setCurrentTheme(ThemeManager::Theme theme)
{
    if (_currentTheme != theme) {
        _currentTheme = theme;
        _themeManager->setCurrentTheme(theme);
        emit currentThemeChanged();
    }
}

void ThemeController::applyThemeToWindow(QQuickWindow* window)
{
    _themeManager->applyTheme(window);
}

QColor ThemeController::getColor(const QString& colorName)
{
    return _themeManager->getColor(colorName);
}

ThemeController* Controller::themeController() const
{
    return _themeController;
}

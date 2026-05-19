#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QGuiApplication>
#include <QClipboard>
#include <QByteArray>
#include <QFile>
#include "stdafx.h"
#include "../src/theme/ThemeManager.h"

#include <QStandardPaths>
#include <QTextStream>
#include <QDebug>
#include <QDir>
#include <QQuickWindow>
#include <QStringList>

class Setting : public QObject
{
    Q_OBJECT
    Q_PROPERTY_AUTO(QString,apiServer);
    Q_PROPERTY_AUTO(QString,apiKey);
    Q_PROPERTY_AUTO(QString,model);
    Q_PROPERTY_AUTO(QString,shortCut);
    Q_PROPERTY_AUTO(QString,provider);
    Q_PROPERTY_AUTO(QString,openaiApiServer);
    Q_PROPERTY_AUTO(QString,openaiApiKey);
    Q_PROPERTY_AUTO(QString,openaiModel);
    Q_PROPERTY_AUTO(QString,ollamaApiServer);
    Q_PROPERTY_AUTO(QString,ollamaModel);
public:
    explicit Setting(QObject *parent = nullptr);
    Q_INVOKABLE bool loadConfig();
    Q_INVOKABLE void updateConfig();
    Q_INVOKABLE void applyProviderConfig(QString provider);

private:
    void applyActiveProviderConfig();
    QString _configPath;
};

class ThemeController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ThemeManager::Theme currentTheme READ currentTheme WRITE setCurrentTheme NOTIFY currentThemeChanged)
public:
    explicit ThemeController(QObject *parent = nullptr);

    ThemeManager::Theme currentTheme() const;
    void setCurrentTheme(ThemeManager::Theme theme);

    Q_INVOKABLE void applyThemeToWindow(QQuickWindow* window);
    Q_INVOKABLE QColor getColor(const QString& colorName);

signals:
    void currentThemeChanged();

private:
    ThemeManager* _themeManager;
    ThemeManager::Theme _currentTheme;
};

class Controller : public QObject
{
    Q_OBJECT
    Q_PROPERTY_AUTO(QString,responseData);
    Q_PROPERTY_AUTO(QString,responseError);
    Q_PROPERTY_AUTO(QString,transToLang);
    Q_PROPERTY_AUTO(bool,isRequesting);
    Q_PROPERTY_AUTO(QString,apiServer);
    Q_PROPERTY_AUTO(QString,apiKey);
    Q_PROPERTY_AUTO(QString,model);
    Q_PROPERTY_AUTO(QString,provider);
    Q_PROPERTY_AUTO(QStringList,availableModels);
    Q_PROPERTY_AUTO(bool,isDetectingModels);
    Q_PROPERTY_AUTO(QString,modelDetectError);
    Q_PROPERTY(ThemeController* themeController READ themeController CONSTANT)


public:
    explicit Controller(QObject *parent = nullptr);

    Q_INVOKABLE void sendMessage(QString str, int mode);
    Q_INVOKABLE void abort();
    Q_INVOKABLE void detectModels(QString apiServer, QString apiKey, QString provider);

    ThemeController* themeController() const;

signals:

private slots:
    void streamReceived();
private:
    QNetworkAccessManager* networkManager;
    QNetworkReply* reply;
    QNetworkReply* modelReply;
    QJsonObject createMessage(const QString& role,const QString& content);
    QString _data;
    std::tuple<QString, bool> _getContent(QString &str);
    std::tuple<QString, bool> _parseResponse(QByteArray &ba);

    QString _getError(QString &str);
    QUrl _buildModelListUrl(const QString& apiServer, const QString& provider);
    QUrl _buildChatCompletionUrl(const QString& apiServer) const;
    QStringList _parseModelList(const QByteArray& data, const QString& provider, QString* errorMessage);

    ThemeController* _themeController;


};

#endif // CONTROLLER_H

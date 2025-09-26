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

class Setting : public QObject
{
    Q_OBJECT
    Q_PROPERTY_AUTO(QString,apiServer);
    Q_PROPERTY_AUTO(QString,apiKey);
    Q_PROPERTY_AUTO(QString,model);
    Q_PROPERTY_AUTO(QString,shortCut);
public:
    explicit Setting(QObject *parent = nullptr);
    Q_INVOKABLE bool loadConfig();
    Q_INVOKABLE void updateConfig();

private:
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
    Q_PROPERTY(ThemeController* themeController READ themeController CONSTANT)


public:
    explicit Controller(QObject *parent = nullptr);

    Q_INVOKABLE void sendMessage(QString str, int mode);
    Q_INVOKABLE void abort();

    ThemeController* themeController() const;

signals:

private slots:
    void streamReceived();
private:
    QNetworkAccessManager* networkManager;
    QNetworkReply* reply;
    QJsonObject createMessage(const QString& role,const QString& content);
    QString _data;
    std::tuple<QString, bool> _getContent(QString &str);
    std::tuple<QString, bool> _parseResponse(QByteArray &ba);

    QString _getError(QString &str);

    ThemeController* _themeController;


};

#endif // CONTROLLER_H

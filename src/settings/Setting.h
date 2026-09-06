#ifndef SETTING_H
#define SETTING_H

#include <QObject>
#include <QString>

#include "stdafx.h"

class Setting : public QObject
{
    Q_OBJECT
    Q_PROPERTY_AUTO(QString, apiServer);
    Q_PROPERTY_AUTO(QString, apiKey);
    Q_PROPERTY_AUTO(QString, model);
    Q_PROPERTY_AUTO(QString, shortCut);
    Q_PROPERTY_AUTO(QString, provider);
    Q_PROPERTY_AUTO(QString, thinkingMode);
    Q_PROPERTY_AUTO(QString, openaiApiServer);
    Q_PROPERTY_AUTO(QString, openaiApiKey);
    Q_PROPERTY_AUTO(QString, openaiModel);
    Q_PROPERTY_AUTO(QString, ollamaApiServer);
    Q_PROPERTY_AUTO(QString, ollamaModel);

public:
    explicit Setting(QObject* parent = nullptr);

    Q_INVOKABLE bool loadConfig();
    Q_INVOKABLE void updateConfig();
    Q_INVOKABLE void applyProviderConfig(QString provider);

private:
    void applyActiveProviderConfig();

    QString m_configPath;
};

#endif // SETTING_H

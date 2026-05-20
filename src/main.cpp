#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QLocale>
#include <QTranslator>

#include "settings/Setting.h"
#include "updater.h"
#include <QIcon>

#include "input/GlobalShortcutManager.h"
#include "input/SelectionTextProvider.h"
#include "providers/ModelDiscoveryService.h"
#include "translation/TranslationService.h"

int main(int argc, char *argv[])
{

    #ifdef Q_OS_WIN
       qDebug() << "Current OS: Windows";
    #endif

    #ifdef Q_OS_MAC
       qDebug() << "Current OS: macOS";
    #endif

    #ifdef Q_OS_LINUX
       qDebug() << "Current OS: Linux";
    #endif

    #ifdef Q_OS_LINUX
        qputenv("QT_QUICK_BACKEND","software");//Failed to build graphics pipeline state under linux, need to be software
    #endif

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon("qrc:///res/logo/logo.ico"));


    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "TransAI_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }

    #ifdef APP_VERSION
    app.setApplicationVersion(APP_VERSION);
    #else
    app.setApplicationVersion("NOT FOUND");
    #endif

    Setting * setting = new Setting();
    qmlRegisterType<Updater>("Updater",1,0,"APIUpdater");
    qmlRegisterType<TranslationService>("TransAI", 1, 0, "TranslationService");
    qmlRegisterType<ModelDiscoveryService>("TransAI", 1, 0, "ModelDiscoveryService");
    qmlRegisterType<GlobalShortcutManager>("TransAI", 1, 0, "GlobalShortcutManager");
    qmlRegisterType<SelectionTextProvider>("TransAI", 1, 0, "SelectionTextProvider");


    QQmlApplicationEngine engine;
    const QUrl url(u"qrc:///qml/main.qml"_qs);
    engine.rootContext()->setContextProperty("setting", setting);
    engine.rootContext()->setContextProperty("app", &app);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}

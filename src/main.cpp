#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QLocale>
#include <QTranslator>

#include "app/ApplicationCommandServer.h"
#include "settings/Setting.h"
#include <QIcon>

#include "input/GlobalShortcutManager.h"
#include "input/SelectionTextProvider.h"
#include "providers/ModelDiscoveryService.h"
#include "translation/TranslationService.h"

int main(int argc, char *argv[])
{
    #ifdef Q_OS_LINUX
        // Software rendering works around GPU pipeline failures on some Linux
        // systems. Users can still override by exporting QT_QUICK_BACKEND
        // before launching (e.g. QT_QUICK_BACKEND=quick for hardware access).
        if (!qEnvironmentVariableIsSet("QT_QUICK_BACKEND")) {
            qputenv("QT_QUICK_BACKEND", "software");
        }
    #endif

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("TransAI");
    // Keep the original AppConfigLocation (~/.config/TransAI on Linux).
    // Adding an organization name would silently select a new settings folder.
    app.setWindowIcon(QIcon("qrc:///res/logo/logo.ico"));

    #ifdef APP_VERSION
    app.setApplicationVersion(APP_VERSION);
    #else
    app.setApplicationVersion("NOT FOUND");
    #endif

    QCommandLineParser commandLine;
    commandLine.setApplicationDescription("AI translation utility");
    commandLine.addHelpOption();
    commandLine.addVersionOption();
    const QCommandLineOption translateSelectionOption(
        QStringList {"translate-selection"},
        "Translate the current Wayland primary selection.");
    const QCommandLineOption backgroundOption(
        QStringList {"background"},
        "Start the application in the background.");
    commandLine.addOption(translateSelectionOption);
    commandLine.addOption(backgroundOption);
    commandLine.process(app);

    const bool translateSelection = commandLine.isSet(translateSelectionOption);
    const bool startHidden = translateSelection || commandLine.isSet(backgroundOption);
    const QString forwardedCommand = translateSelection ? "translate-selection" : (startHidden ? "noop" : "show");

    if (ApplicationCommandServer::sendCommandToRunningInstance(forwardedCommand)) {
        return 0;
    }

    ApplicationCommandServer commandServer;
    if (!commandServer.start()) {
        // Another instance may have started listening after our initial attempt.
        if (ApplicationCommandServer::sendCommandToRunningInstance(forwardedCommand)) {
            return 0;
        }
        qCritical() << "Failed to start the application command server:" << commandServer.errorString();
        return 1;
    }


    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "TransAI_" + QLocale(locale).name();
        // NOTE: no .qm files are bundled yet (source strings live in
        // TransAI_zh_CN.ts). This loop degrades gracefully until translations
        // are compiled into the resource.
        if (translator.load(":/i18n/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }

    Setting * setting = new Setting(&app);
    qmlRegisterType<TranslationService>("TransAI", 1, 0, "TranslationService");
    qmlRegisterType<ModelDiscoveryService>("TransAI", 1, 0, "ModelDiscoveryService");
    qmlRegisterType<GlobalShortcutManager>("TransAI", 1, 0, "GlobalShortcutManager");
    qmlRegisterType<SelectionTextProvider>("TransAI", 1, 0, "SelectionTextProvider");


    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:///qml/main.qml"));
    engine.rootContext()->setContextProperty("setting", setting);
    engine.rootContext()->setContextProperty("app", &app);
    engine.rootContext()->setContextProperty("appCommandServer", &commandServer);
    engine.rootContext()->setContextProperty("startHidden", startHidden);
    engine.rootContext()->setContextProperty("initialCommand", translateSelection ? "translate-selection" : "");

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}

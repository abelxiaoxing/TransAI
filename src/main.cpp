#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QNetworkProxyFactory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWindow>

#include "app/ApplicationCommandServer.h"
#include "app/ClipboardHelper.h"
#include "app/TranslationHistory.h"
#include "app/WindowDismissHelper.h"
#include "settings/Setting.h"
#include <QIcon>

#include "input/GlobalShortcutManager.h"
#include "input/GlobalClickMonitor.h"
#include "input/SelectionTextProvider.h"
#include "capture/ScreenshotCaptureService.h"
#include "ocr/OcrEngine.h"
#include "providers/ModelDiscoveryService.h"
#include "translation/TranslationService.h"
#include "vision/VisionTranslationService.h"

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

    // Honour the OS proxy configuration (corporate / campus networks). Qt
    // defaults to no proxy at all otherwise. localhost queries resolve to
    // Direct on all supported platforms, so a local Ollama is unaffected.
    QNetworkProxyFactory::setUseSystemConfiguration(true);
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
    const QCommandLineOption translateScreenshotOption(
        QStringList {"translate-screenshot"},
        "Capture a screenshot and translate it with a vision model.");
    const QCommandLineOption backgroundOption(
        QStringList {"background"},
        "Start the application in the background.");
    commandLine.addOption(translateSelectionOption);
    commandLine.addOption(translateScreenshotOption);
    commandLine.addOption(backgroundOption);
    commandLine.process(app);

    const bool translateSelection = commandLine.isSet(translateSelectionOption);
    const bool translateScreenshot = !translateSelection && commandLine.isSet(translateScreenshotOption);
    const bool startHidden = translateSelection || translateScreenshot || commandLine.isSet(backgroundOption);
    // A second --background launch forwards "noop", which the running instance
    // simply swallows (its only purpose is to prove the socket is alive); a
    // plain launch forwards "show" to raise the existing window.
    const QString initialCommand = translateSelection ? QStringLiteral("translate-selection")
        : translateScreenshot ? QStringLiteral("translate-screenshot")
        : QString();
    const QString forwardedCommand = !initialCommand.isEmpty() ? initialCommand : (startHidden ? "noop" : "show");

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


    // I18n lives in QML (the I18n singleton reads Setting.displayLanguage),
    // so no QTranslator is installed here.

    Setting * setting = new Setting(&app);
    WindowDismissHelper * windowDismiss = new WindowDismissHelper(&app);
    GlobalClickMonitor * clickMonitor = new GlobalClickMonitor(&app);
    ClipboardHelper * clipboard = new ClipboardHelper(&app);
    TranslationHistory * translationHistory = new TranslationHistory(&app);
    // OCR backend for the screenshot OCR path (tesseract CLI on
    // Linux/Windows, Vision framework on macOS); always a valid object.
    OcrEngine * ocrEngine = createOcrEngine().release();
    ocrEngine->setParent(&app);
    qmlRegisterType<TranslationService>("TransAI", 1, 0, "TranslationService");
    qmlRegisterType<VisionTranslationService>("TransAI", 1, 0, "VisionTranslationService");
    qmlRegisterType<ScreenshotCaptureService>("TransAI", 1, 0, "ScreenshotCaptureService");
    qmlRegisterType<ModelDiscoveryService>("TransAI", 1, 0, "ModelDiscoveryService");
    qmlRegisterType<GlobalShortcutManager>("TransAI", 1, 0, "GlobalShortcutManager");
    qmlRegisterType<SelectionTextProvider>("TransAI", 1, 0, "SelectionTextProvider");


    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:///qml/main.qml"));
    engine.rootContext()->setContextProperty("setting", setting);
    engine.rootContext()->setContextProperty("appCommandServer", &commandServer);
    engine.rootContext()->setContextProperty("windowDismiss", windowDismiss);
    engine.rootContext()->setContextProperty("globalClickMonitor", clickMonitor);
    engine.rootContext()->setContextProperty("clipboard", clipboard);
    engine.rootContext()->setContextProperty("translationHistory", translationHistory);
    engine.rootContext()->setContextProperty("ocrEngine", ocrEngine);
    engine.rootContext()->setContextProperty("startHidden", startHidden);
    engine.rootContext()->setContextProperty("initialCommand", initialCommand);

    // QML 加载/创建失败必须退出：本进程已持有单实例 socket，若滞留会变成
    // 无窗口的僵尸实例，吞掉之后所有 TransAI 命令（表现为“启动无反应”）。
    // Qt 6.4+ 下根组件编译失败走 objectCreationFailed；objectCreated(nullptr)
    // 保留以覆盖实例化失败路径。
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [url](const QUrl &objUrl) {
        if (url == objUrl) {
            qCritical() << "Failed to load" << url << "— exiting to avoid a headless single-instance squat";
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    // The evdev click monitor only sees that a physical press happened; pairing
    // it with presses delivered to our own window is what tells an outside
    // click from an in-window one (e.g. clicks on an open in-window dropdown).
    if (!engine.rootObjects().isEmpty()) {
        if (QWindow *rootWindow = qobject_cast<QWindow *>(engine.rootObjects().first())) {
            windowDismiss->trackWindow(rootWindow);
        }
    }

    return app.exec();
}

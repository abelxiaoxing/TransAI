#include <QFile>
#include <QGuiApplication>
#include <QObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtTest>

// Exercises the I18n.qml singleton outside the app UI: table lookup,
// placeholder substitution, missing-key fallback, and — most importantly —
// live re-translation when Setting.displayLanguage changes.
//
// The singleton is staged into a temp module directory (qmldir + I18n.qml +
// a generated harness) so the directory's implicit import resolves it the
// same way the app's qml/ directory does.

namespace {
const char kHarnessQml[] = R"qml(import QtQuick

QtObject {
    readonly property string language: I18n.language
    readonly property string plain: I18n.t("Settings")
    readonly property string placeholder: I18n.t("%1 models detected — pick one from the dropdown.", 3)
    readonly property string missing: I18n.t("No such key in the table")
}
)qml";
}

// Minimal stand-in for the Setting context property; I18n only reads
// displayLanguage from it.
class StubSetting : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString displayLanguage MEMBER language NOTIFY displayLanguageChanged)

public:
    QString language = QStringLiteral("en");

    void setLanguage(const QString& value)
    {
        if (language == value) {
            return;
        }
        language = value;
        emit displayLanguageChanged();
    }

signals:
    void displayLanguageChanged();
};

class TestI18n : public QObject
{
    Q_OBJECT

private slots:
    void lookupPlaceholderFallbackAndLiveSwitch();

private:
    bool stageModule(const QString& dir) const;
};

bool TestI18n::stageModule(const QString& dir) const
{
    const QString source = QStringLiteral(I18N_QML_SOURCE);
    if (!QFile::copy(source, dir + "/I18n.qml")) {
        return false;
    }

    QFile qmldir(dir + "/qmldir");
    if (!qmldir.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    qmldir.write("singleton I18n 1.0 I18n.qml\n");
    qmldir.close();

    QFile harness(dir + "/harness.qml");
    if (!harness.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    harness.write(kHarnessQml);
    return true;
}

void TestI18n::lookupPlaceholderFallbackAndLiveSwitch()
{
    QTemporaryDir staging;
    QVERIFY(staging.isValid());
    QVERIFY(stageModule(staging.path()));

    StubSetting stub;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("setting", &stub);

    QQmlComponent component(&engine, QUrl::fromLocalFile(staging.path() + "/harness.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> harness(component.create());
    QVERIFY2(harness, qPrintable(component.errorString()));

    // English (the default here): source text passes through untouched.
    QCOMPARE(harness->property("language").toString(), QStringLiteral("en"));
    QCOMPARE(harness->property("plain").toString(), QStringLiteral("Settings"));
    QCOMPARE(harness->property("placeholder").toString(),
             QStringLiteral("3 models detected — pick one from the dropdown."));
    QCOMPARE(harness->property("missing").toString(), QStringLiteral("No such key in the table"));

    // Switch to Chinese: every dependent binding re-evaluates.
    stub.setLanguage(QStringLiteral("zh-CN"));
    QCOMPARE(harness->property("language").toString(), QStringLiteral("zh-CN"));
    QCOMPARE(harness->property("plain").toString(), QStringLiteral("设置"));
    QCOMPARE(harness->property("placeholder").toString(),
             QStringLiteral("已检测到 3 个模型——请从下拉列表中选择。"));
    // Missing keys fall back to the English source text.
    QCOMPARE(harness->property("missing").toString(), QStringLiteral("No such key in the table"));

    // And back again.
    stub.setLanguage(QStringLiteral("en"));
    QCOMPARE(harness->property("plain").toString(), QStringLiteral("Settings"));
}

int main(int argc, char* argv[])
{
    // Offscreen: the engine may probe the platform plugin while loading QML.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    TestI18n test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_i18n.moc"

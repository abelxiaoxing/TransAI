#include <QtTest>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include "settings/Setting.h"

// Round-trip tests for the Setting persistence layer. QStandardPaths test
// mode redirects AppConfigLocation to an isolated per-test-binary directory,
// so the real user config is never touched.
class TestSetting : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void defaultsForFreshConfig();
    void targetLanguageRoundTrip();
    void unknownTargetLanguageIsPreserved();

private:
    static QString configPath()
    {
        return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
            .filePath(QStringLiteral("config.json"));
    }
};

void TestSetting::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestSetting::init()
{
    QFile::remove(configPath());
}

void TestSetting::defaultsForFreshConfig()
{
    const Setting setting;
    QCOMPARE(setting.targetLanguage(), QString());
}

void TestSetting::targetLanguageRoundTrip()
{
    {
        Setting setting;
        setting.targetLanguage(QStringLiteral("Japanese"));
        setting.updateConfig();
    }
    {
        const Setting setting;
        QCOMPARE(setting.targetLanguage(), QStringLiteral("Japanese"));
    }
}

void TestSetting::unknownTargetLanguageIsPreserved()
{
    // Unknown language names are preserved verbatim (the QML language list
    // owns the fallback), but the value must round-trip as stored.
    QDir().mkpath(QFileInfo(configPath()).absolutePath());
    {
        QFile file(configPath());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(R"({"configVersion": 2, "targetLanguage": "Klingon"})");
    }
    const Setting setting;
    QCOMPARE(setting.targetLanguage(), QStringLiteral("Klingon"));
}

QTEST_MAIN(TestSetting)
#include "tst_setting.moc"

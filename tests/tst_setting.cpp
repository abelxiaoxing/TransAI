#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "settings/Setting.h"
#include "settings/SecretStore.h"

namespace {
// In-memory SecretStore for deterministic keychain-integration tests.
class FakeSecretStore : public SecretStore
{
public:
    bool available = true;
    QHash<QString, QString> values;

    bool isAvailable() override { return available; }
    QString read(const QString& key) override { return values.value(key); }
    bool write(const QString& key, const QString& value) override
    {
        values.insert(key, value);
        return true;
    }
    bool remove(const QString& key) override
    {
        values.remove(key);
        return true;
    }
};
} // namespace

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
    void translationModeRoundTrip();
    void invalidValuesFallBackToDefaults();
    void secretStoreWinsOverConfigPlaintext();
    void plaintextKeyMigratedAndCleared();
    void saveWritesStoreAndDropsPlaintext();
    void unavailableStoreKeepsPlaintext();
    void emptyKeyRemovesFromStore();

private:
    static QString configPath()
    {
        return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
            .filePath(QStringLiteral("config.json"));
    }
    static QJsonObject readConfig()
    {
        QFile file(configPath());
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(file.readAll()).object();
    }
    static void writeConfig(const QByteArray& json)
    {
        QDir().mkpath(QFileInfo(configPath()).absolutePath());
        QFile file(configPath());
        file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        file.write(json);
        file.close();
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
    Setting setting;
    QCOMPARE(setting.targetLanguage(), QString());
    QCOMPARE(setting.translationMode(), 0);
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

void TestSetting::translationModeRoundTrip()
{
    {
        Setting setting;
        setting.translationMode(2);
        setting.updateConfig();
    }
    {
        const Setting setting;
        QCOMPARE(setting.translationMode(), 2);
    }
}

void TestSetting::invalidValuesFallBackToDefaults()
{
    // Hand-written config with an out-of-range translationMode; the enum has    // exactly 0..2, anything else must clamp back to Translate (0).
    QDir().mkpath(QFileInfo(configPath()).absolutePath());
    {
        QFile file(configPath());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(R"({"configVersion": 2, "translationMode": 7, "targetLanguage": "Klingon"})");
    }
    {
        const Setting setting;
        QCOMPARE(setting.translationMode(), 0);
        // Unknown language names are preserved verbatim (the QML language
        // list owns the fallback), but the value must round-trip as stored.
        QCOMPARE(setting.targetLanguage(), QStringLiteral("Klingon"));
    }

    // Negative mode values clamp too.
    {
        QFile file(configPath());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(R"({"configVersion": 2, "translationMode": -1})");
    }
    {
        const Setting setting;
        QCOMPARE(setting.translationMode(), 0);
    }
}

void TestSetting::secretStoreWinsOverConfigPlaintext()
{
    writeConfig(R"({"configVersion": 2, "openaiApiKey": "sk-plain"})");

    auto fake = std::make_unique<FakeSecretStore>();
    fake->values.insert(QStringLiteral("openaiApiKey"), QStringLiteral("sk-store"));
    FakeSecretStore* store = fake.get();

    const Setting setting(nullptr, std::move(fake));
    // The keychain value wins over the config plaintext.
    QCOMPARE(setting.openaiApiKey(), QStringLiteral("sk-store"));
    QCOMPARE(store->values.value(QStringLiteral("openaiApiKey")), QStringLiteral("sk-store"));
}

void TestSetting::plaintextKeyMigratedAndCleared()
{
    writeConfig(R"({"configVersion": 2, "openaiApiKey": "sk-plain"})");

    auto fake = std::make_unique<FakeSecretStore>();
    FakeSecretStore* store = fake.get();

    const Setting setting(nullptr, std::move(fake));
    QCOMPARE(setting.openaiApiKey(), QStringLiteral("sk-plain"));
    // Migrated into the store …
    QCOMPARE(store->values.value(QStringLiteral("openaiApiKey")), QStringLiteral("sk-plain"));
    // … and the constructor's re-save dropped the plaintext from the file.
    const QJsonObject obj = readConfig();
    QCOMPARE(obj.value(QStringLiteral("openaiApiKey")).toString(), QString());
    QCOMPARE(obj.value(QStringLiteral("openaiApiKeyInSecretStore")).toBool(), true);
}

void TestSetting::saveWritesStoreAndDropsPlaintext()
{
    auto fake = std::make_unique<FakeSecretStore>();
    FakeSecretStore* store = fake.get();

    QHash<QString, QString> storedValues;
    {
        Setting setting(nullptr, std::move(fake));
        setting.openaiApiKey(QStringLiteral("sk-new"));
        setting.updateConfig();
        QCOMPARE(readConfig().value(QStringLiteral("openaiApiKey")).toString(), QString());
        QCOMPARE(store->values.value(QStringLiteral("openaiApiKey")), QStringLiteral("sk-new"));
        // The store is owned by Setting; snapshot before it is destroyed.
        storedValues = store->values;
    }
    {
        // Reload through an identical (still-populated) fake store.
        auto fake2 = std::make_unique<FakeSecretStore>();
        fake2->values = storedValues;
        const Setting reloaded(nullptr, std::move(fake2));
        QCOMPARE(reloaded.openaiApiKey(), QStringLiteral("sk-new"));
    }
}

void TestSetting::unavailableStoreKeepsPlaintext()
{
    auto fake = std::make_unique<FakeSecretStore>();
    fake->available = false;

    {
        Setting setting(nullptr, std::move(fake));
        setting.openaiApiKey(QStringLiteral("sk-plain"));
        setting.updateConfig();
    }
    // No usable backend → legacy plaintext round-trip, marker stays false.
    const QJsonObject obj = readConfig();
    QCOMPARE(obj.value(QStringLiteral("openaiApiKey")).toString(), QStringLiteral("sk-plain"));
    QCOMPARE(obj.value(QStringLiteral("openaiApiKeyInSecretStore")).toBool(), false);

    auto fake2 = std::make_unique<FakeSecretStore>();
    fake2->available = false;
    const Setting reloaded(nullptr, std::move(fake2));
    QCOMPARE(reloaded.openaiApiKey(), QStringLiteral("sk-plain"));
}

void TestSetting::emptyKeyRemovesFromStore()
{
    // A config file must exist so the constructor takes the loadConfig path
    // (otherwise it starts from defaults and saves immediately).
    writeConfig(R"({"configVersion": 2})");

    auto fake = std::make_unique<FakeSecretStore>();
    fake->values.insert(QStringLiteral("openaiApiKey"), QStringLiteral("sk-old"));
    FakeSecretStore* store = fake.get();

    Setting setting(nullptr, std::move(fake));
    QCOMPARE(setting.openaiApiKey(), QStringLiteral("sk-old"));

    setting.openaiApiKey(QString());
    setting.updateConfig();
    QVERIFY(!store->values.contains(QStringLiteral("openaiApiKey")));
    QCOMPARE(readConfig().value(QStringLiteral("openaiApiKey")).toString(), QString());
}

QTEST_MAIN(TestSetting)
#include "tst_setting.moc"

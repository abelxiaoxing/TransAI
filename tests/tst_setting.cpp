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
    // Counts how often the keychain is touched: saveConfig() runs on every
    // settings edit, so unchanged keys must not round-trip to the backend.
    int writeCount = 0;
    int readCount = 0;
    int removeCount = 0;

    bool isAvailable() override { return available; }
    QString read(const QString& key) override
    {
        ++readCount;
        return values.value(key);
    }
    bool write(const QString& key, const QString& value) override
    {
        ++writeCount;
        values.insert(key, value);
        return true;
    }
    bool remove(const QString& key) override
    {
        ++removeCount;
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
    void unknownLanguageNameIsPreserved();
    void secretStoreWinsOverConfigPlaintext();
    void plaintextKeyMigratedAndCleared();
    void saveWritesStoreAndDropsPlaintext();
    void unavailableStoreKeepsPlaintext();
    void emptyKeyRemovesFromStore();
    void unchangedKeySkipsKeychainWrite();
    void failedKeychainWriteIsRetriedOnNextSave();
    void legacyTopLevelKeysAreMigrated();
    void newerConfigVersionIsTolerated();
    void windowGeometryIsClampedToSharedBounds();

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
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(json);
            file.close();
        }
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

void TestSetting::unknownLanguageNameIsPreserved()
{
    // Unknown language names are preserved verbatim (the QML language list
    // owns the fallback), but the value must round-trip as stored.
    QDir().mkpath(QFileInfo(configPath()).absolutePath());
    {
        QFile file(configPath());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(R"({"configVersion": 2, "targetLanguage": "Klingon"})");
    }
    {
        const Setting setting;
        QCOMPARE(setting.targetLanguage(), QStringLiteral("Klingon"));
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

void TestSetting::unchangedKeySkipsKeychainWrite()
{
    auto fake = std::make_unique<FakeSecretStore>();
    FakeSecretStore* store = fake.get();
    Setting setting(nullptr, std::move(fake));

    setting.openaiApiKey(QStringLiteral("sk-stable"));
    setting.updateConfig();
    QCOMPARE(store->writeCount, 1);

    // Language/geometry edits all funnel through updateConfig(); none of them
    // may re-write an unchanged secret.
    for (int i = 0; i < 5; ++i) {
        setting.targetLanguage(QStringLiteral("English"));
        setting.updateConfig();
        setting.windowWidth(900 + i);
        setting.updateConfig();
    }
    QCOMPARE(store->writeCount, 1);
    QCOMPARE(store->removeCount, 0);
    QVERIFY(setting.apiKeyStoredSecurely());
}

void TestSetting::failedKeychainWriteIsRetriedOnNextSave()
{
    auto fake = std::make_unique<FakeSecretStore>();
    FakeSecretStore* store = fake.get();
    Setting setting(nullptr, std::move(fake));

    // Store becomes unavailable mid-session: the key falls back to plaintext,
    // is not marked as stored, and is retried (not silently skipped) later.
    store->available = false;
    setting.openaiApiKey(QStringLiteral("sk-retry"));
    setting.updateConfig();
    QVERIFY(!setting.apiKeyStoredSecurely());
    QCOMPARE(readConfig().value(QStringLiteral("openaiApiKey")).toString(), QStringLiteral("sk-retry"));

    store->available = true;
    setting.updateConfig();
    QVERIFY(setting.apiKeyStoredSecurely());
    QCOMPARE(store->values.value(QStringLiteral("openaiApiKey")), QStringLiteral("sk-retry"));
    QCOMPARE(readConfig().value(QStringLiteral("openaiApiKey")).toString(), QString());
}

void TestSetting::legacyTopLevelKeysAreMigrated()
{
    // Pre-per-provider config (no configVersion): the top-level keys belong to
    // the OpenAI-compatible provider and must be lifted on load.
    writeConfig(R"({
        "provider": "openai",
        "apiServer": "https://legacy.example.com",
        "apiKey": "sk-legacy",
        "model": "legacy-model"
    })");

    const Setting setting;
    QCOMPARE(setting.openaiApiServer(), QStringLiteral("https://legacy.example.com"));
    QCOMPARE(setting.openaiApiKey(), QStringLiteral("sk-legacy"));
    QCOMPARE(setting.openaiModel(), QStringLiteral("legacy-model"));
    // Version-2 keys win over the legacy leftovers.
    QCOMPARE(setting.apiServer(), QStringLiteral("https://legacy.example.com"));
}

void TestSetting::newerConfigVersionIsTolerated()
{
    // A config written by a future release must still load (forward compatible
    // for the keys this version knows about) instead of being discarded.
    writeConfig(R"({"configVersion": 99, "openaiModel": "future-model", "ollamaModel": "llama3"})");

    const Setting setting;
    QCOMPARE(setting.openaiModel(), QStringLiteral("future-model"));
    QCOMPARE(setting.ollamaModel(), QStringLiteral("llama3"));
}

void TestSetting::windowGeometryIsClampedToSharedBounds()
{
    // Values below the minimum / above the maximum are treated as "not set".
    writeConfig(R"({"configVersion": 2, "windowWidth": 100, "windowHeight": 100000})");
    {
        const Setting setting;
        QCOMPARE(setting.windowWidth(), 0);
        QCOMPARE(setting.windowHeight(), 0);
    }

    writeConfig(R"({"configVersion": 2, "windowWidth": 800, "windowHeight": 900})");
    {
        const Setting setting;
        QCOMPARE(setting.windowWidth(), 800);
        QCOMPARE(setting.windowHeight(), 900);
        // QML sizes itself from these; they must stay inside the clamps.
        QVERIFY(setting.defaultWindowWidth() >= setting.minWindowWidth());
        QVERIFY(setting.defaultWindowWidth() <= setting.maxWindowWidth());
        QVERIFY(setting.defaultWindowHeight() >= setting.minWindowHeight());
        QVERIFY(setting.defaultWindowHeight() <= setting.maxWindowHeight());
    }
}

QTEST_MAIN(TestSetting)
#include "tst_setting.moc"

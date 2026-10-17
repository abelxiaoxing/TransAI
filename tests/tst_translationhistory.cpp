#include <QtTest>

#include "app/TranslationHistory.h"

// Round-trip and capacity tests for TranslationHistory, isolated through
// QStandardPaths test mode so the real user config directory is untouched.
class TestTranslationHistory : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void addAndReload();
    void blankResultsIgnored();
    void capacityCapped();
    void clearEmpties();
    void screenshotEntriesHaveEmptySource();
};

void TestTranslationHistory::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestTranslationHistory::addAndReload()
{
    {
        TranslationHistory history;
        QVERIFY(history.available());
        history.addEntry(QStringLiteral("text"), QStringLiteral("hello"),
                         QStringLiteral("你好"), QStringLiteral("简体中文"));
        QCOMPARE(history.entries().size(), 1);
        QCOMPARE(history.entries().first().toMap().value(QStringLiteral("source")).toString(),
                 QStringLiteral("hello"));
    }
    {
        // A fresh instance reads the persisted file back.
        TranslationHistory reloaded;
        QCOMPARE(reloaded.entries().size(), 1);
        const QVariantMap entry = reloaded.entries().first().toMap();
        QCOMPARE(entry.value(QStringLiteral("source")).toString(), QStringLiteral("hello"));
        QCOMPARE(entry.value(QStringLiteral("result")).toString(), QStringLiteral("你好"));
        QCOMPARE(entry.value(QStringLiteral("type")).toString(), QStringLiteral("text"));
        QVERIFY(!entry.value(QStringLiteral("timestamp")).toString().isEmpty());
    }
}

void TestTranslationHistory::blankResultsIgnored()
{
    TranslationHistory history;
    history.clear();
    history.addEntry(QStringLiteral("text"), QStringLiteral("hello"), QStringLiteral("  "), QString());
    QCOMPARE(history.entries().size(), 0);
}

void TestTranslationHistory::capacityCapped()
{
    TranslationHistory history;
    history.clear();
    for (int i = 0; i < TranslationHistory::maxEntries() + 5; ++i) {
        history.addEntry(QStringLiteral("text"), QStringLiteral("src%1").arg(i),
                         QStringLiteral("res%1").arg(i), QString());
    }
    QCOMPARE(history.entries().size(), TranslationHistory::maxEntries());
    // Newest first: the oldest 5 entries were dropped.
    QCOMPARE(history.entries().first().toMap().value(QStringLiteral("source")).toString(),
             QStringLiteral("src%1").arg(TranslationHistory::maxEntries() + 4));
    history.clear();
}

void TestTranslationHistory::clearEmpties()
{
    TranslationHistory history;
    history.addEntry(QStringLiteral("text"), QStringLiteral("a"), QStringLiteral("b"), QString());
    history.clear();
    QCOMPARE(history.entries().size(), 0);

    TranslationHistory reloaded;
    QCOMPARE(reloaded.entries().size(), 0);
}

void TestTranslationHistory::screenshotEntriesHaveEmptySource()
{
    TranslationHistory history;
    history.clear();
    history.addEntry(QStringLiteral("screenshot"), QString(),
                     QStringLiteral("translated text"), QStringLiteral("English"));
    QCOMPARE(history.entries().size(), 1);
    const QVariantMap entry = history.entries().first().toMap();
    QCOMPARE(entry.value(QStringLiteral("type")).toString(), QStringLiteral("screenshot"));
    QCOMPARE(entry.value(QStringLiteral("source")).toString(), QString());
    history.clear();
}

QTEST_GUILESS_MAIN(TestTranslationHistory)
#include "tst_translationhistory.moc"

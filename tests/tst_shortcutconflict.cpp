#include "input/ShortcutConflict.h"

#include <QTest>

// The two global hotkeys (selection / screenshot) used to be able to share one
// chord: QHotkey accepts a duplicate registration and then fires both actions
// on a single press. GlobalShortcutManager now arbitrates through
// findConflictingAction(), which this suite exercises without needing a
// display or a real hotkey backend.
class TestShortcutConflict : public QObject {
    Q_OBJECT

private slots:
    void emptyRegistryHasNoConflict();
    void differentChordsDoNotConflict();
    void sameActionReplacesItself();
    void duplicateChordReportsTheOwner();
    void modifierOrderAndCaseAreNormalized();
    void emptyCandidateIsNeverAConflict();
    void emptyRegisteredValuesAreIgnored();
};

void TestShortcutConflict::emptyRegistryHasNoConflict() {
    const QHash<QString, QString> registered;
    QCOMPARE(findConflictingAction(registered, "translateSelection", "Ctrl+Alt+T"), QString());
}

void TestShortcutConflict::differentChordsDoNotConflict() {
    QHash<QString, QString> registered;
    registered.insert("translateSelection", "Ctrl+Alt+T");
    QCOMPARE(findConflictingAction(registered, "translateScreenshot", "Ctrl+Alt+S"), QString());
    // Same key, different modifiers is a different chord.
    QCOMPARE(findConflictingAction(registered, "translateScreenshot", "Ctrl+Shift+T"), QString());
}

void TestShortcutConflict::sameActionReplacesItself() {
    QHash<QString, QString> registered;
    registered.insert("translateSelection", "Ctrl+Alt+T");
    QCOMPARE(findConflictingAction(registered, "translateSelection", "Ctrl+Alt+T"), QString());
}

void TestShortcutConflict::duplicateChordReportsTheOwner() {
    QHash<QString, QString> registered;
    registered.insert("translateSelection", "Ctrl+Alt+T");
    QCOMPARE(findConflictingAction(registered, "translateScreenshot", "Ctrl+Alt+T"),
             QStringLiteral("translateSelection"));

    // And the other direction, with a third action present.
    registered.insert("translateScreenshot", "Ctrl+Alt+S");
    QCOMPARE(findConflictingAction(registered, "someFutureAction", "Ctrl+Alt+S"),
             QStringLiteral("translateScreenshot"));
    QCOMPARE(findConflictingAction(registered, "translateSelection", "Ctrl+Alt+S"),
             QStringLiteral("translateScreenshot"));
}

void TestShortcutConflict::modifierOrderAndCaseAreNormalized() {
    QHash<QString, QString> registered;
    registered.insert("translateSelection", "Ctrl+Shift+A");
    QCOMPARE(findConflictingAction(registered, "translateScreenshot", "Shift+Ctrl+A"),
             QStringLiteral("translateSelection"));
    QCOMPARE(findConflictingAction(registered, "translateScreenshot", "ctrl+shift+a"),
             QStringLiteral("translateSelection"));
}

void TestShortcutConflict::emptyCandidateIsNeverAConflict() {
    QHash<QString, QString> registered;
    registered.insert("translateSelection", "Ctrl+Alt+T");
    QCOMPARE(findConflictingAction(registered, "translateScreenshot", QString()), QString());
}

void TestShortcutConflict::emptyRegisteredValuesAreIgnored() {
    // A cleared action keeps its key in the map with an empty value.
    QHash<QString, QString> registered;
    registered.insert("translateSelection", QString());
    QCOMPARE(findConflictingAction(registered, "translateScreenshot", "Ctrl+Alt+T"), QString());
}

QTEST_MAIN(TestShortcutConflict)
#include "tst_shortcutconflict.moc"

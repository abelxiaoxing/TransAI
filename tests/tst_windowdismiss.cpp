#include <QGuiApplication>
#include <QMouseEvent>
#include <QWindow>
#include <QtTest>

#include "app/WindowDismissHelper.h"

// Covers the "was the deactivation caused by a click?" machinery that keeps the
// main window alive under focus-follows-mouse compositors: presses delivered to
// our own window are timestamped, so the evdev click-outside path can pair (and
// discard) them.
class TestWindowDismiss : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void noPressRecordedYet();
    void windowPressIsTimestamped();
    void nonPressEventsAreIgnored();
    void offscreenPlatformNeverAutoHides();
    void untrackedNullWindowIsIgnored();
};

void TestWindowDismiss::initTestCase() {
    // The whole suite runs headless; WindowDismissHelper keys its policy off
    // the platform name, so offscreen is also the "unknown platform" case.
    QCOMPARE(QGuiApplication::platformName(), QStringLiteral("offscreen"));
}

void TestWindowDismiss::noPressRecordedYet() {
    const WindowDismissHelper helper;
    QCOMPARE(helper.msecsSinceLastWindowPress(), qint64(-1));
}

void TestWindowDismiss::windowPressIsTimestamped() {
    QWindow window;
    WindowDismissHelper helper;
    helper.trackWindow(&window);

    QMouseEvent press(QEvent::MouseButtonPress, QPointF(12, 12), QPointF(12, 12), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &press);

    const qint64 elapsed = helper.msecsSinceLastWindowPress();
    QVERIFY2(elapsed >= 0 && elapsed < 200, qPrintable(QString::number(elapsed)));
}

void TestWindowDismiss::nonPressEventsAreIgnored() {
    QWindow window;
    WindowDismissHelper helper;
    helper.trackWindow(&window);

    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(12, 12), QPointF(12, 12), Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &release);

    // Only presses stamp the time; a release must not fake a press pairing.
    QCOMPARE(helper.msecsSinceLastWindowPress(), qint64(-1));
}

void TestWindowDismiss::offscreenPlatformNeverAutoHides() {
    const WindowDismissHelper helper;
    // -1 = pointer never left the window; large = left long ago. Neither may
    // hide on a platform we do not understand.
    QVERIFY(!helper.shouldHideOnDeactivate(-1));
    QVERIFY(!helper.shouldHideOnDeactivate(5000));
    QVERIFY(!helper.shouldHideOnDeactivate(0));
}

void TestWindowDismiss::untrackedNullWindowIsIgnored() {
    WindowDismissHelper helper;
    helper.trackWindow(nullptr);  // must not crash
    QCOMPARE(helper.msecsSinceLastWindowPress(), qint64(-1));
}

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    TestWindowDismiss test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_windowdismiss.moc"

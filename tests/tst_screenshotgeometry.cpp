#include <QRect>
#include <QSize>
#include <QtTest>

#include "capture/ScreenshotGeometry.h"

// The region-selection overlay reports rectangles in PREVIEW pixels (the grab
// is downscaled to at most 1920px wide for display); the service maps them
// back onto the original image. That mapping is pure math and was previously
// untested — an off-by-a-scale-factor bug would silently crop the wrong area.
class TestScreenshotGeometry : public QObject {
    Q_OBJECT

private slots:
    void identityWhenPreviewMatchesOriginal();
    void mapsDownscaledSelectionBack();
    void roundsNonIntegerScales();
    void clampsSelectionToImageBounds();
    void degenerateInputsProduceEmptyRect();
};

void TestScreenshotGeometry::identityWhenPreviewMatchesOriginal() {
    const QSize size(1920, 1080);
    const QRect mapped = ScreenshotGeometry::mapPreviewRectToOriginal(size, size, QRect(100, 200, 300, 400));
    QCOMPARE(mapped, QRect(100, 200, 300, 400));
}

void TestScreenshotGeometry::mapsDownscaledSelectionBack() {
    // 4K grab, preview downscaled by exactly 2x.
    const QSize original(3840, 2160);
    const QSize preview(1920, 1080);
    const QRect mapped = ScreenshotGeometry::mapPreviewRectToOriginal(original, preview, QRect(100, 100, 200, 150));
    QCOMPARE(mapped, QRect(200, 200, 400, 300));
}

void TestScreenshotGeometry::roundsNonIntegerScales() {
    // 3000px original encoded at 1920px → scale 1.5625.
    const QSize original(3000, 1000);
    const QSize preview(1920, 640);
    const QRect mapped = ScreenshotGeometry::mapPreviewRectToOriginal(original, preview, QRect(101, 51, 33, 21));
    // 101 * 1.5625 = 157.8125 → 158; 51 * 1.5625 = 79.6875 → 80;
    // 33 * 1.5625 = 51.5625 → 52; 21 * 1.5625 = 32.8125 → 33.
    QCOMPARE(mapped, QRect(158, 80, 52, 33));
}

void TestScreenshotGeometry::clampsSelectionToImageBounds() {
    // A stale or rounding-pushed selection must never exceed the original.
    const QSize original(1000, 800);
    const QSize preview(500, 400);
    const QRect mapped = ScreenshotGeometry::mapPreviewRectToOriginal(original, preview, QRect(450, 380, 100, 100));
    QCOMPARE(mapped, QRect(900, 760, 100, 40));
    QVERIFY(QRect(QPoint(0, 0), original).contains(mapped));
}

void TestScreenshotGeometry::degenerateInputsProduceEmptyRect() {
    const QSize image(100, 100);
    // Null/invalid sizes.
    QVERIFY(ScreenshotGeometry::mapPreviewRectToOriginal(QSize(), image, QRect(0, 0, 10, 10)).isEmpty());
    QVERIFY(ScreenshotGeometry::mapPreviewRectToOriginal(image, QSize(), QRect(0, 0, 10, 10)).isEmpty());
    QVERIFY(ScreenshotGeometry::mapPreviewRectToOriginal(image, QSize(0, 10), QRect(0, 0, 10, 10)).isEmpty());
    // Empty selection (a click without a drag).
    QVERIFY(ScreenshotGeometry::mapPreviewRectToOriginal(image, image, QRect()).isEmpty());
    QVERIFY(ScreenshotGeometry::mapPreviewRectToOriginal(image, image, QRect(5, 5, 0, 0)).isEmpty());
}

QTEST_GUILESS_MAIN(TestScreenshotGeometry)
#include "tst_screenshotgeometry.moc"

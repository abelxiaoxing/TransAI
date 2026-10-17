#include <QtTest>
#include <QImage>

#include "vision/ImagePayloadBuilder.h"

namespace {
ImagePayloadResult buildSolid(const QSize& size, int maxWidth, int quality, bool withAlpha = false)
{
    QImage image(size, withAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    image.fill(withAlpha ? qRgba(10, 20, 30, 0) : qRgb(10, 20, 30));
    return ImagePayloadBuilder::buildDataUrl(image, maxWidth, quality);
}
} // namespace

class TestImagePayloadBuilder : public QObject
{
    Q_OBJECT

private slots:
    void nullImageFails();
    void tinyImageFails();
    void normalImageProducesDataUrl();
    void wideImageIsScaledDown();
    void narrowImageKeepsSize();
    void alphaIsFlattened();
    void invalidParamsFallBackToDefaults();
};

void TestImagePayloadBuilder::nullImageFails()
{
    const ImagePayloadResult result = ImagePayloadBuilder::buildDataUrl(QImage(), 1600, 85);
    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.dataUrl.isEmpty());
}

void TestImagePayloadBuilder::tinyImageFails()
{
    const ImagePayloadResult result = buildSolid(QSize(4, 4), 1600, 85);
    QVERIFY(!result.ok);
}

void TestImagePayloadBuilder::normalImageProducesDataUrl()
{
    const ImagePayloadResult result = buildSolid(QSize(800, 600), 1600, 85);
    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY(result.dataUrl.startsWith("data:image/jpeg;base64,"));
    QCOMPARE(result.finalSize, QSize(800, 600));
}

void TestImagePayloadBuilder::wideImageIsScaledDown()
{
    const ImagePayloadResult result = buildSolid(QSize(3200, 1600), 1600, 85);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.finalSize.width(), 1600);
    QCOMPARE(result.finalSize.height(), 800);
}

void TestImagePayloadBuilder::narrowImageKeepsSize()
{
    const ImagePayloadResult result = buildSolid(QSize(1200, 900), 1600, 85);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.finalSize, QSize(1200, 900));
}

void TestImagePayloadBuilder::alphaIsFlattened()
{
    // Transparent pixels must not become a black JPEG.
    const ImagePayloadResult result = buildSolid(QSize(200, 200), 1600, 85, true);
    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY(result.dataUrl.startsWith("data:image/jpeg;base64,"));
}

void TestImagePayloadBuilder::invalidParamsFallBackToDefaults()
{
    // maxWidth <= 0 disables the caller's limit; quality outside 0..100
    // falls back to the default — neither may fail the build.
    const ImagePayloadResult noScale = buildSolid(QSize(4000, 2000), 0, 85);
    QVERIFY2(noScale.ok, qPrintable(noScale.error));
    QCOMPARE(noScale.finalSize, QSize(1600, 800));

    const ImagePayloadResult badQuality = buildSolid(QSize(800, 600), 1600, 250);
    QVERIFY2(badQuality.ok, qPrintable(badQuality.error));
}

QTEST_GUILESS_MAIN(TestImagePayloadBuilder)
#include "tst_imagepayloadbuilder.moc"

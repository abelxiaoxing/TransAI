#include <QImage>
#include <QSize>
#include <QUrl>
#include <QtTest>

#include "app/ScreenshotPreviewProvider.h"

// What the input panel shows for a vision-model screenshot session: the
// screenshot itself, served to QML as `image://transai-screenshot/<id>`.
//
// The contract matters in two places the QML side cannot express: every publish
// must produce a *new* URL (QML caches by URL, so a reused one would show the
// previous screenshot), and the provider must hand back the latest image at
// whatever size the panel asks for.
class TestScreenshotPreview : public QObject {
    Q_OBJECT

private slots:
    void publishesUniqueUrls();
    void servesLatestImage();
    void downscalesButNeverUpscales();
    void returnsNullBeforeAnyPublish();
};

namespace {

QImage solidImage(int width, int height) {
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    return image;
}

}  // namespace

void TestScreenshotPreview::publishesUniqueUrls() {
    ScreenshotPreviewStore store;
    const QUrl first = store.publish(solidImage(100, 50));
    const QUrl second = store.publish(solidImage(100, 50));

    QCOMPARE(first.scheme(), QStringLiteral("image"));
    QCOMPARE(first.host(), QStringLiteral("transai-screenshot"));
    QVERIFY(!first.path().isEmpty());
    // The id doubles as the cache buster.
    QVERIFY2(first != second, qPrintable(first.toString()));
}

void TestScreenshotPreview::servesLatestImage() {
    ScreenshotPreviewStore store;
    store.publish(solidImage(120, 40));
    const QImage stored = solidImage(200, 30);
    store.publish(stored);

    ScreenshotPreviewProvider provider(&store);
    QSize size;
    const QImage served = provider.requestImage(QStringLiteral("2"), &size, QSize());
    QCOMPARE(served.size(), QSize(200, 30));
    QCOMPARE(size, QSize(200, 30));
}

void TestScreenshotPreview::downscalesButNeverUpscales() {
    ScreenshotPreviewStore store;
    store.publish(solidImage(400, 200));

    ScreenshotPreviewProvider provider(&store);
    QSize size;
    const QImage small = provider.requestImage(QStringLiteral("1"), &size, QSize(100, 100));
    QCOMPARE(small.size(), QSize(100, 50));  // aspect ratio kept
    QCOMPARE(size, QSize(400, 200));  // full size reported, not the scaled one

    const QImage big = provider.requestImage(QStringLiteral("1"), &size, QSize(2000, 2000));
    QCOMPARE(big.size(), QSize(400, 200));  // never upscaled
}

void TestScreenshotPreview::returnsNullBeforeAnyPublish() {
    ScreenshotPreviewStore store;
    ScreenshotPreviewProvider provider(&store);
    QSize size(7, 7);
    QVERIFY(provider.requestImage(QStringLiteral("1"), &size, QSize()).isNull());
    QCOMPARE(size, QSize(0, 0));
}

QTEST_GUILESS_MAIN(TestScreenshotPreview)
#include "tst_screenshotpreview.moc"

#ifndef SCREENSHOTPREVIEWPROVIDER_H
#define SCREENSHOTPREVIEWPROVIDER_H

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>
#include <QSize>
#include <QUrl>

// What the input panel shows for a vision-model screenshot session.
//
// Why: the OCR path recognizes the screenshot and fills the input panel with the
// text that is being translated, but a vision request has no text source at
// all — the panel then kept whatever draft was typed before, so the top of the
// window showed "text" that had nothing to do with the translation below it. The
// panel now shows the screenshot itself (and a click returns to plain text
// editing with the draft intact).
//
// Two objects because QQuickImageProvider is not a QObject (and already brings
// its own base): `ScreenshotPreviewStore` is the QML-facing context property
// (`screenshotPreview.publish(image)`) holding the latest image, and the
// provider serves it to `Image { source: "image://transai-screenshot/<id>" }` on
// the scene-graph thread — hence the mutex.
class ScreenshotPreviewStore : public QObject {
    Q_OBJECT

public:
    explicit ScreenshotPreviewStore(QObject* parent = nullptr);

    // Stores a downscaled copy of `image` and returns a fresh URL for it. A new
    // id per call matters: QML caches images by URL, so reusing one would show
    // the previous screenshot.
    Q_INVOKABLE QUrl publish(const QImage& image);

    QImage image() const;

    // Widest stored preview in pixels: the panel is a few hundred pixels wide,
    // so keeping a full 4K screenshot alive would only waste memory.
    static constexpr int kMaxPreviewWidth = 900;

private:
    mutable QMutex m_mutex;
    QImage m_image;
    quint64 m_generation = 0;
};

class ScreenshotPreviewProvider : public QQuickImageProvider {
public:
    explicit ScreenshotPreviewProvider(const ScreenshotPreviewStore* store);

    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override;

private:
    const ScreenshotPreviewStore* m_store;
};

#endif  // SCREENSHOTPREVIEWPROVIDER_H

#include "app/ScreenshotPreviewProvider.h"

#include <QMutexLocker>

ScreenshotPreviewStore::ScreenshotPreviewStore(QObject* parent)
    : QObject(parent) {}

QUrl ScreenshotPreviewStore::publish(const QImage& image) {
    QMutexLocker locker(&m_mutex);
    ++m_generation;

    QImage stored = image;
    if (stored.width() > kMaxPreviewWidth) {
        stored = stored.scaledToWidth(kMaxPreviewWidth, Qt::SmoothTransformation);
    }
    m_image = stored;
    locker.unlock();

    // `image://<providerId>/<id>`: the id only has to be unique per published
    // screenshot, and it doubles as the cache buster (see publish()).
    return QUrl(QStringLiteral("image://transai-screenshot/%1").arg(m_generation));
}

QImage ScreenshotPreviewStore::image() const {
    QMutexLocker locker(&m_mutex);
    return m_image;
}

ScreenshotPreviewProvider::ScreenshotPreviewProvider(const ScreenshotPreviewStore* store)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_store(store) {}

QImage ScreenshotPreviewProvider::requestImage(const QString& id, QSize* size, const QSize& requested) {
    Q_UNUSED(id);

    const QImage image = m_store->image();
    if (size) {
        *size = image.size();
    }
    if (image.isNull()) {
        return image;
    }
    // `requested` is what the QML Image declared; honour a downscale but never
    // upscale (a tiny region screenshot must not be blown up and blurred).
    if (requested.isValid() && requested.width() > 0 && requested.width() < image.width()) {
        return image.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

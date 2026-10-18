#include "capture/ScreenshotCaptureService.h"

#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QScreen>
#include <QVariantList>
#include <QVariantMap>

#include "capture/PortalScreenshotBackend.h"
#include "capture/ScreenshotGeometry.h"
#include "vision/ImagePayloadBuilder.h"

namespace {
bool isWaylandSession()
{
    return QGuiApplication::platformName().toLower().contains(QLatin1String("wayland"));
}

// Preview encoding for the region-selection overlay: capped at 1920px wide
// (mapping math in finishRegionSelection compensates), JPEG quality 80 —
// the preview is display-only, the crop always comes from the original grab.
constexpr int kPreviewMaxWidth = 1920;
constexpr int kPreviewJpegQuality = 80;
} // namespace

ScreenshotCaptureService::ScreenshotCaptureService(QObject* parent)
    : QObject(parent)
{
}

void ScreenshotCaptureService::captureInteractive()
{
    if (m_captureInProgress) {
        return;
    }

#ifdef Q_OS_LINUX
    if (isWaylandSession()) {
        if (!m_portalBackend) {
            m_portalBackend = new PortalScreenshotBackend(this);
            connect(m_portalBackend, &PortalScreenshotBackend::imageCaptured, this, [this](const QImage& image) {
                setCaptureInProgress(false);
                emit imageCaptured(image);
            });
            connect(m_portalBackend, &PortalScreenshotBackend::captureCanceled, this, [this]() {
                setCaptureInProgress(false);
                emit captureCanceled();
            });
            connect(m_portalBackend, &PortalScreenshotBackend::captureFailed, this, [this](const QString& reason) {
                setCaptureInProgress(false);
                emit captureFailed(reason);
            });
        }
        setCaptureInProgress(true);
        m_portalBackend->captureInteractive();
        // Synchronous failure (bus down, portal missing) already reset the flag.
        return;
    }
#endif

    // Non-Wayland: grab every screen, then hand over to the in-app region
    // selection overlay (RegionSelectorWindow) via regionSelectionRequested().
    clearGrabbedScreens();

    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (!screen) {
            continue;
        }
        const QImage image = screen->grabWindow(0).toImage();
        if (image.isNull()) {
            continue; // e.g. headless/offscreen platforms
        }
        const ImagePayloadResult preview = ImagePayloadBuilder::buildDataUrl(image, kPreviewMaxWidth, kPreviewJpegQuality);
        if (!preview.ok) {
            continue;
        }
        m_grabbedImages.append(image);
        m_previewSizes.append(preview.finalSize);
        m_previewDataUrls.append(QString::fromLatin1(preview.dataUrl));
    }

    if (m_grabbedImages.isEmpty()) {
        emit captureFailed(QStringLiteral("Screen capture is not available on this platform"));
        return;
    }

    setCaptureInProgress(true);
    emit regionSelectionRequested();
}

QVariantList ScreenshotCaptureService::grabbedScreenInfos()
{
    QVariantList infos;
    infos.reserve(m_grabbedImages.size());
    for (int i = 0; i < m_grabbedImages.size(); ++i) {
        QVariantMap info;
        info.insert(QStringLiteral("previewWidth"), m_previewSizes.at(i).width());
        info.insert(QStringLiteral("previewHeight"), m_previewSizes.at(i).height());
        // Hand the (potentially multi-megabyte) base64 preview over to QML and
        // drop our copy: the Image source owns it from here on, and the crop
        // always comes from the original grab.
        info.insert(QStringLiteral("dataUrl"), m_previewDataUrls.at(i));
    }
    m_previewDataUrls.clear();
    return infos;
}

void ScreenshotCaptureService::finishRegionSelection(int screenIndex, int x, int y, int width, int height)
{
    if (screenIndex < 0 || screenIndex >= m_grabbedImages.size()) {
        return; // Stale/foreign call; not a user-visible error.
    }
    if (width < 1 || height < 1) {
        clearGrabbedScreens();
        setCaptureInProgress(false);
        emit captureCanceled();
        return;
    }

    const QImage original = m_grabbedImages.at(screenIndex);
    const QSize preview = m_previewSizes.at(screenIndex);

    // Map preview-pixel coordinates back onto the original image (pure
    // helper, unit-tested in tst_screenshotgeometry).
    const QRect bounded = ScreenshotGeometry::mapPreviewRectToOriginal(
        original.size(), preview, QRect(x, y, width, height));

    const QImage cropped = bounded.isEmpty() ? QImage() : original.copy(bounded);
    clearGrabbedScreens();
    setCaptureInProgress(false);

    if (cropped.isNull()) {
        emit captureFailed(QStringLiteral("The selected region is outside the captured screen"));
        return;
    }
    emit imageCaptured(cropped);
}

void ScreenshotCaptureService::cancelRegionSelection()
{
    if (m_grabbedImages.isEmpty()) {
        return; // Not in a region-selection session.
    }
    clearGrabbedScreens();
    setCaptureInProgress(false);
    emit captureCanceled();
}

void ScreenshotCaptureService::setCaptureInProgress(bool inProgress)
{
    if (m_captureInProgress == inProgress) {
        return;
    }
    m_captureInProgress = inProgress;
    emit captureInProgressChanged();
}

void ScreenshotCaptureService::clearGrabbedScreens()
{
    m_grabbedImages.clear();
    m_previewSizes.clear();
    m_previewDataUrls.clear();
}

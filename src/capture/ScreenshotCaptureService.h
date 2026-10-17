#ifndef SCREENSHOT_CAPTURE_SERVICE_H
#define SCREENSHOT_CAPTURE_SERVICE_H

#include <QImage>
#include <QObject>

class PortalScreenshotBackend;

// Unified screenshot entry point for QML.
//
// Platform strategy:
//   - Wayland (Linux): xdg-desktop-portal Screenshot (interactive system UI)
//     via PortalScreenshotBackend — the only reliable way to capture other
//     applications' content there.
//   - Other platforms (X11, Windows, macOS): grab every screen, then let the
//     user pick a region in the in-app fullscreen overlay (RegionSelectorWindow).
//
// The service only produces a QImage; it knows nothing about OCR or
// translation. User cancellation is reported via captureCanceled() and is
// not an error.
class ScreenshotCaptureService : public QObject
{
    Q_OBJECT
    // True while a capture is in progress (portal wait or in-app region
    // selection); the UI uses it to avoid stacking requests.
    Q_PROPERTY(bool captureInProgress READ captureInProgress NOTIFY captureInProgressChanged)

public:
    explicit ScreenshotCaptureService(QObject* parent = nullptr);

    bool captureInProgress() const { return m_captureInProgress; }

    Q_INVOKABLE void captureInteractive();

    // In-app region selection protocol (non-Wayland path). After
    // regionSelectionRequested(), QML reads grabbedScreenInfos() to build the
    // overlays, then answers with exactly one of finish/cancel.
    // Each info: {previewWidth, previewHeight, dataUrl} — the preview may be
    // smaller than the original grab; finishRegionSelection() receives rect
    // in PREVIEW pixels and maps it back to the original image.
    Q_INVOKABLE QVariantList grabbedScreenInfos();
    Q_INVOKABLE void finishRegionSelection(int screenIndex, int x, int y, int width, int height);
    Q_INVOKABLE void cancelRegionSelection();

signals:
    void captureInProgressChanged();
    void imageCaptured(QImage image);
    void captureCanceled();
    void captureFailed(QString reason);
    // Emitted on the non-Wayland path once the screens are grabbed; QML
    // should open the region-selection overlays.
    void regionSelectionRequested();

private:
    void setCaptureInProgress(bool inProgress);
    void clearGrabbedScreens();

    PortalScreenshotBackend* m_portalBackend = nullptr;
    bool m_captureInProgress = false;
    // Original grabs and their preview encodings, parallel arrays, valid
    // between regionSelectionRequested() and finish/cancel.
    QList<QImage> m_grabbedImages;
    QList<QSize> m_previewSizes;
    QList<QString> m_previewDataUrls;
};

#endif // SCREENSHOT_CAPTURE_SERVICE_H

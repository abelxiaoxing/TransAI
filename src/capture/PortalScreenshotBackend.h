#ifndef PORTAL_SCREENSHOT_BACKEND_H
#define PORTAL_SCREENSHOT_BACKEND_H

#include <QImage>
#include <QObject>
#include <QString>

class QTimer;

// Wayland screenshot via xdg-desktop-portal
// (org.freedesktop.portal.Screenshot, D-Bus). The system portal UI handles
// region/window/fullscreen selection, which is the only reliable way to
// capture other applications' content on Wayland (QScreen::grabWindow cannot).
//
// Responsibilities: issue the Screenshot call, wait for the portal Request
// Response signal, load the returned file into a QImage. It knows nothing
// about OCR or translation. User cancellation is reported via
// captureCanceled() and is not an error.
//
// Only functional on Linux (Qt DBus); on other platforms every capture
// immediately fails with a clear reason.
class PortalScreenshotBackend : public QObject
{
    Q_OBJECT

public:
    explicit PortalScreenshotBackend(QObject* parent = nullptr);
    ~PortalScreenshotBackend() override;

    // True when a session bus exists and the portal service is registered.
    // Cheap enough to call on every capture; result is not cached.
    static bool isAvailable();

    void captureInteractive();

signals:
    void imageCaptured(QImage image);
    void captureCanceled();
    void captureFailed(QString reason);

private slots:
    // Connected (string-based, per request path) to the portal Request
    // Response signal for the current call.
    void onPortalResponse(uint response, const QVariantMap& results);

private:
    void finishCapture();   // disconnect signal + stop timeout, reset state

    QString m_requestPath;
    QTimer* m_timeoutTimer = nullptr;
};

#endif // PORTAL_SCREENSHOT_BACKEND_H

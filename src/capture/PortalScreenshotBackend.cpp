#include "capture/PortalScreenshotBackend.h"

#include <QImage>
#include <QTimer>

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUrl>
#include <QUuid>
#endif

namespace {
// Portal Screenshot without a response is almost always a stuck portal UI;
// cap the wait so the app never stays in limbo.
constexpr int kPortalTimeoutMs = 3 * 60 * 1000;
}

PortalScreenshotBackend::PortalScreenshotBackend(QObject* parent)
    : QObject(parent)
{
}

PortalScreenshotBackend::~PortalScreenshotBackend()
{
    finishCapture();
}

bool PortalScreenshotBackend::isAvailable()
{
#ifdef Q_OS_LINUX
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }
    QDBusConnectionInterface* iface = bus.interface();
    return iface && iface->isServiceRegistered(QStringLiteral("org.freedesktop.portal.Desktop"));
#else
    return false;
#endif
}

void PortalScreenshotBackend::captureInteractive()
{
#ifdef Q_OS_LINUX
    if (!m_requestPath.isEmpty()) {
        emit captureFailed(QStringLiteral("A screenshot request is already in progress"));
        return;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        emit captureFailed(QStringLiteral("D-Bus session bus is not available"));
        return;
    }
    if (!isAvailable()) {
        emit captureFailed(QStringLiteral("xdg-desktop-portal is not available on this system"));
        return;
    }

    // Precompute the expected request path so the Response signal can be
    // connected BEFORE the call goes out (the portal may answer quickly).
    // The token becomes a D-Bus object path element, so dashes are invalid;
    // strip them from the UUID.
    QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    token.remove(QLatin1Char('-'));
    QString senderPath = bus.baseService();
    senderPath.remove(QLatin1Char(':'));
    senderPath.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString requestPath = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2")
        .arg(senderPath, token);

    bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"), requestPath,
                QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                this, SLOT(onPortalResponse(uint,QVariantMap)));

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    // interactive=true lets the user pick a region/window in the system UI.
    options.insert(QStringLiteral("interactive"), true);

    const QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.Screenshot"),
        QStringLiteral("Screenshot"));
    QDBusMessage message = call;
    message.setArguments({QString(), options});

    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, requestPath](QDBusPendingCallWatcher* self) {
        self->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *self;
        if (reply.isError()) {
            finishCapture();
            emit captureFailed(QStringLiteral("Portal screenshot call failed: %1").arg(reply.error().message()));
            return;
        }
        // The portal may route the response through a different handle than
        // our precomputed token; accept it and re-point the connection.
        const QString actualPath = reply.value().path();
        if (actualPath != requestPath) {
            QDBusConnection bus = QDBusConnection::sessionBus();
            bus.disconnect(QStringLiteral("org.freedesktop.portal.Desktop"), requestPath,
                           QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                           this, SLOT(onPortalResponse(uint,QVariantMap)));
            bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"), actualPath,
                        QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                        this, SLOT(onPortalResponse(uint,QVariantMap)));
        }
        m_requestPath = actualPath;
    });

    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
            finishCapture();
            emit captureFailed(QStringLiteral("Portal screenshot timed out"));
        });
    }
    m_timeoutTimer->start(kPortalTimeoutMs);
#else
    emit captureFailed(QStringLiteral("Portal screenshot is only supported on Linux"));
#endif
}

void PortalScreenshotBackend::onPortalResponse(uint response, const QVariantMap& results)
{
#ifdef Q_OS_LINUX
    // response: 0 = success, 1 = user canceled, 2 = other error.
    if (response == 1) {
        finishCapture();
        emit captureCanceled();
        return;
    }
    if (response != 0) {
        finishCapture();
        emit captureFailed(QStringLiteral("Portal screenshot failed (response %1)").arg(response));
        return;
    }

    const QUrl uri(results.value(QStringLiteral("uri")).toString());
    const QString filePath = uri.isLocalFile() ? uri.toLocalFile() : uri.toString();
    QImage image(filePath);
    if (image.isNull()) {
        finishCapture();
        emit captureFailed(QStringLiteral("Failed to load the captured screenshot file"));
        return;
    }

    finishCapture();
    emit imageCaptured(image);
#else
    Q_UNUSED(response);
    Q_UNUSED(results);
#endif
}

void PortalScreenshotBackend::finishCapture()
{
#ifdef Q_OS_LINUX
    if (!m_requestPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(
            QStringLiteral("org.freedesktop.portal.Desktop"), m_requestPath,
            QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
            this, SLOT(onPortalResponse(uint,QVariantMap)));
        m_requestPath.clear();
    }
#endif
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
}

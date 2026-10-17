#include "WindowDismissHelper.h"

#include <QDateTime>
#include <QEvent>
#include <QGuiApplication>
#include <QWindow>

#if defined(Q_OS_LINUX)
// <QGuiApplication> already pulls in qguiapplication_platform.h, which
// declares QNativeInterface::QX11Application.
#include <X11/Xlib.h>
#endif

namespace {
// Deactivations arriving within this window of a pointer leave are caused by
// the mouse moving away (focus-follows-mouse), not by a click.
constexpr qint64 kPointerLeaveGraceMs = 300;
}

WindowDismissHelper::WindowDismissHelper(QObject *parent)
    : QObject(parent)
{
}

bool WindowDismissHelper::shouldHideOnDeactivate(qint64 msecsSincePointerLeave) const
{
#if defined(Q_OS_LINUX)
    const QString platform = QGuiApplication::platformName();
    if (platform == QStringLiteral("xcb")) {
        return leftButtonPressedX11();
    }
    if (platform.startsWith(QStringLiteral("wayland"))) {
        return msecsSincePointerLeave >= 0 && msecsSincePointerLeave > kPointerLeaveGraceMs;
    }
    // offscreen/minimal and anything else: never auto-hide.
    return false;
#else
    Q_UNUSED(msecsSincePointerLeave);
    return true;
#endif
}

void WindowDismissHelper::trackWindow(QWindow *window)
{
    if (window) {
        window->installEventFilter(this);
    }
}

qint64 WindowDismissHelper::msecsSinceLastWindowPress() const
{
    if (m_lastWindowPressMs < 0) {
        return -1;
    }
    return QDateTime::currentMSecsSinceEpoch() - m_lastWindowPressMs;
}

bool WindowDismissHelper::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::TouchBegin:
    case QEvent::TabletPress:
        m_lastWindowPressMs = QDateTime::currentMSecsSinceEpoch();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

#if defined(Q_OS_LINUX)
bool WindowDismissHelper::leftButtonPressedX11()
{
    auto *x11 = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11 || !x11->display()) {
        // No X connection: fall back to treating deactivation as click-caused.
        return true;
    }
    Display *display = x11->display();
    Window root = 0, child = 0;
    int rootX = 0, rootY = 0, winX = 0, winY = 0;
    unsigned int mask = 0;
    if (!XQueryPointer(display, DefaultRootWindow(display), &root, &child,
                       &rootX, &rootY, &winX, &winY, &mask)) {
        return false;
    }
    return (mask & Button1Mask) != 0;
}
#endif

#ifndef WINDOWDISMISSHELPER_H
#define WINDOWDISMISSHELPER_H

#include <QObject>

class QWindow;

// Decides whether a window deactivation should auto-hide the main window.
//
// The goal is "hide on left-click outside, never on a mere mouse move", but
// the available signal differs per platform:
//  - Windows / macOS: pointer movement never deactivates the window, so a
//    deactivation already implies a click (or app switch) elsewhere.
//  - X11: the X server reports the live button mask, so a click-caused focus
//    loss can be told apart from a focus-follows-mouse one.
//  - Wayland: clients cannot observe clicks outside their own surface at
//    all, and focus-follows-mouse compositors deactivate on pointer leave.
//    main.qml therefore passes how long ago the pointer left the window and
//    only deactivations that are clearly not mouse-move-caused hide it.
class WindowDismissHelper : public QObject
{
    Q_OBJECT
public:
    explicit WindowDismissHelper(QObject *parent = nullptr);

    // msecsSincePointerLeave: milliseconds since the pointer last left the
    // window surface, or -1 when it has not left since the window was shown.
    Q_INVOKABLE bool shouldHideOnDeactivate(qint64 msecsSincePointerLeave) const;

    // --- Click-outside correlation for the evdev GlobalClickMonitor path ---
    // The evdev press only tells us a physical click happened; whether it
    // landed inside our window is something only the window's own event
    // stream knows. Hover state is NOT a reliable substitute: the default
    // Popup.Item captures hover while open, so clicks landing on an
    // in-window dropdown report hovered=false at the window root.
    void trackWindow(QWindow *window);
    // Milliseconds since the last press delivered to the tracked window,
    // or -1 when no press has been observed. QML defers the hide decision
    // briefly after an evdev press (evdev usually wins the race against the
    // compositor-forwarded window event) and then consults this.
    Q_INVOKABLE qint64 msecsSinceLastWindowPress() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    qint64 m_lastWindowPressMs = -1;

#if defined(Q_OS_LINUX)
    static bool leftButtonPressedX11();
#endif
};

#endif // WINDOWDISMISSHELPER_H

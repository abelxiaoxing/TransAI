#ifndef GLOBALCLICKMONITOR_H
#define GLOBALCLICKMONITOR_H

#include <QObject>
#include <QHash>

class QSocketNotifier;
class QTimer;

// Passive observer of physical left-button presses, read straight from the
// evdev device nodes (read-only, never grabs).
//
// Why this exists: under Wayland a client receives NO event whatsoever for
// clicks that land on other windows, and with focus-follows-mouse the plain
// deactivation signal cannot be told apart from a mere mouse move. evdev is
// the only channel where a regular application can still observe the click
// itself (the compositor reads the same nodes via libinput).
//
// Requires read access to pointer devices in /dev/input/ (uaccess rule or
// "input" group — see docs/hyprland-integration.md). Without it, `available`
// is false and QML falls back to Esc / close-button dismissal.
//
// Non-Linux platforms: always unavailable (Windows/macOS never need it —
// there a plain deactivation already means the user clicked elsewhere).
class GlobalClickMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    explicit GlobalClickMonitor(QObject *parent = nullptr);
    ~GlobalClickMonitor() override;

    bool available() const { return !m_notifiersByFd.isEmpty(); }

signals:
    void availableChanged();
    // A physical left mouse button press happened somewhere on the system.
    // QML decides whether the pointer is outside our window at that moment.
    void leftButtonPressed();

private:
    void scanDevices();
    void readEvents(int fd);
    void removeDevice(int fd);

    QHash<int, QSocketNotifier *> m_notifiersByFd;
    QHash<int, QString> m_pathsByFd;
    QTimer *m_rescanTimer = nullptr;
};

#endif // GLOBALCLICKMONITOR_H

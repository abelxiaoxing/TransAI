#include "GlobalClickMonitor.h"

#if defined(Q_OS_LINUX)

#include <QDir>
#include <QSocketNotifier>
#include <QTimer>

#include <algorithm>

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
// Re-scan interval so hot-plugged mice get picked up as well.
constexpr int kRescanIntervalMs = 30 * 1000;

template <typename T> bool testBit(int bit, const T *bits)
{
    constexpr int bitsPerWord = 8 * sizeof(T);
    return (bits[bit / bitsPerWord] >> (bit % bitsPerWord)) & 1;
}

bool deviceHasLeftButton(int fd)
{
    unsigned long keyBits[(KEY_MAX / (8 * sizeof(unsigned long))) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) < 0) {
        return false;
    }
    return testBit(BTN_LEFT, keyBits);
}
}

GlobalClickMonitor::GlobalClickMonitor(QObject *parent)
    : QObject(parent)
{
    scanDevices();
    if (!available()) {
        qWarning("GlobalClickMonitor: /dev/input/event* is not readable; "
                 "click-outside-to-hide is disabled. Grant the session user "
                 "read access to pointer devices (uaccess rule or 'input' "
                 "group, see docs/hyprland-integration.md).");
    }

    m_rescanTimer = new QTimer(this);
    m_rescanTimer->setInterval(kRescanIntervalMs);
    connect(m_rescanTimer, &QTimer::timeout, this, &GlobalClickMonitor::scanDevices);
    m_rescanTimer->start();
}

GlobalClickMonitor::~GlobalClickMonitor()
{
    const auto fds = m_notifiersByFd.keys();
    for (int fd : fds) {
        removeDevice(fd);
    }
}

void GlobalClickMonitor::scanDevices()
{
    const bool wasAvailable = available();

    const QDir dir(QStringLiteral("/dev/input"));
    const QStringList entries = dir.entryList({QStringLiteral("event*")}, QDir::System, QDir::Name);
    for (const QString &entry : entries) {
        const QString path = dir.absoluteFilePath(entry);
        const bool alreadyOpen = std::any_of(m_pathsByFd.cbegin(), m_pathsByFd.cend(),
                                             [&path](const QString &p) { return p == path; });
        if (alreadyOpen) {
            continue;
        }

        const int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue; // No permission (or race) — stay passive.
        }
        if (!deviceHasLeftButton(fd)) {
            ::close(fd);
            continue; // Keyboard etc. — we only watch pointer devices.
        }

        auto *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this, [this, fd]() { readEvents(fd); });
        m_notifiersByFd.insert(fd, notifier);
        m_pathsByFd.insert(fd, path);
        qInfo("GlobalClickMonitor: watching %s", qPrintable(path));
    }

    if (available() != wasAvailable) {
        emit availableChanged();
    }
}

void GlobalClickMonitor::readEvents(int fd)
{
    for (;;) {
        struct input_event events[16];
        const ssize_t bytes = ::read(fd, events, sizeof(events));
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            removeDevice(fd); // Device unplugged or revoked.
            return;
        }
        if (bytes == 0) {
            return;
        }

        const int count = static_cast<int>(bytes / sizeof(input_event));
        for (int i = 0; i < count; ++i) {
            const input_event &ev = events[i];
            if (ev.type == EV_KEY && ev.code == BTN_LEFT && ev.value == 1) {
                emit leftButtonPressed();
            }
        }
        if (bytes < static_cast<ssize_t>(sizeof(events))) {
            return; // Kernel gave us everything it had.
        }
    }
}

void GlobalClickMonitor::removeDevice(int fd)
{
    QSocketNotifier *notifier = m_notifiersByFd.take(fd);
    if (notifier) {
        notifier->setEnabled(false);
        notifier->deleteLater();
    }
    m_pathsByFd.remove(fd);
    ::close(fd);
}

#else // !Q_OS_LINUX

GlobalClickMonitor::GlobalClickMonitor(QObject *parent)
    : QObject(parent)
{
}

GlobalClickMonitor::~GlobalClickMonitor() = default;
void GlobalClickMonitor::scanDevices() {}
void GlobalClickMonitor::readEvents(int) {}
void GlobalClickMonitor::removeDevice(int) {}

#endif // Q_OS_LINUX

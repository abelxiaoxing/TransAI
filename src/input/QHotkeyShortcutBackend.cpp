#include "input/QHotkeyShortcutBackend.h"

#include <QGuiApplication>

QHotkeyShortcutBackend::QHotkeyShortcutBackend(QObject* parent)
    : ShortcutBackend(parent)
{
}

QHotkeyShortcutBackend::~QHotkeyShortcutBackend()
{
    const QStringList actionIds = m_hotkeys.keys();
    for (const QString& actionId : actionIds) {
        unregisterShortcut(actionId);
    }
}

bool QHotkeyShortcutBackend::registerShortcut(const QString& actionId, const QKeySequence& sequence)
{
    if (actionId.trimmed().isEmpty()) {
        emit failed(actionId, "Action id is empty");
        return false;
    }

    if (QGuiApplication::platformName().contains("wayland", Qt::CaseInsensitive)) {
        emit failed(actionId, "QHotkey backend is not enabled on Wayland");
        return false;
    }

    unregisterShortcut(actionId);

    QHotkey* hotkey = new QHotkey(sequence, true, this);
    connect(hotkey, &QHotkey::activated, this, [this, actionId]() {
        emit activated(actionId);
    });

    if (!hotkey->isRegistered()) {
        const QString reason = QString("Failed to register shortcut: %1").arg(sequence.toString());
        hotkey->deleteLater();
        emit failed(actionId, reason);
        return false;
    }

    m_hotkeys.insert(actionId, hotkey);
    return true;
}

void QHotkeyShortcutBackend::unregisterShortcut(const QString& actionId)
{
    QHotkey* hotkey = m_hotkeys.take(actionId);
    if (!hotkey) {
        return;
    }
    hotkey->setRegistered(false);
    hotkey->deleteLater();
}

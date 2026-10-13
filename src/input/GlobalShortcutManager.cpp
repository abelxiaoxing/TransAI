#include "input/GlobalShortcutManager.h"

#include <QGuiApplication>
#include <QKeySequence>

GlobalShortcutManager::GlobalShortcutManager(QObject* parent)
    : QObject(parent)
{
}

GlobalShortcutManager::~GlobalShortcutManager()
{
    const QStringList actionIds = m_hotkeys.keys();
    for (const QString& actionId : actionIds) {
        unregisterShortcut(actionId);
    }
}

bool GlobalShortcutManager::platformSupportsHotkeys()
{
    // Global hotkeys are unavailable on Wayland; QHotkey's X11 backend also
    // crashes under headless platforms (offscreen/minimal), so refuse there
    // too. Real sessions report xcb / windows / cocoa.
    const QString platform = QGuiApplication::platformName().toLower();
    return !(platform == QLatin1String("wayland") || platform == QLatin1String("offscreen")
             || platform == QLatin1String("minimal") || platform == QLatin1String("dummy"));
}

bool GlobalShortcutManager::registerShortcut(const QString& actionId, const QString& sequence)
{
    const QKeySequence keySequence(sequence);
    if (keySequence.isEmpty()) {
        emit shortcutRegisterFailed(actionId, "Shortcut is empty");
        return false;
    }
    if (actionId.trimmed().isEmpty()) {
        emit shortcutRegisterFailed(actionId, "Action id is empty");
        return false;
    }

    if (!platformSupportsHotkeys()) {
        emit shortcutRegisterFailed(actionId,
                                    QString("Global hotkeys are not available on %1").arg(QGuiApplication::platformName()));
        return false;
    }

    unregisterShortcut(actionId);

    QHotkey* hotkey = new QHotkey(keySequence, true, this);
    connect(hotkey, &QHotkey::activated, this, [this, actionId]() {
        emit shortcutActivated(actionId);
    });

    if (!hotkey->isRegistered()) {
        const QString reason = QString("Failed to register shortcut: %1").arg(keySequence.toString());
        hotkey->deleteLater();
        emit shortcutRegisterFailed(actionId, reason);
        return false;
    }

    m_hotkeys.insert(actionId, hotkey);
    return true;
}

void GlobalShortcutManager::unregisterShortcut(const QString& actionId)
{
    QHotkey* hotkey = m_hotkeys.take(actionId);
    if (!hotkey) {
        return;
    }
    hotkey->setRegistered(false);
    hotkey->deleteLater();
}

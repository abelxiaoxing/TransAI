#include "input/GlobalShortcutManager.h"

#include "input/ShortcutConflict.h"

#include <QGuiApplication>
#include <QKeySequence>

GlobalShortcutManager::GlobalShortcutManager(QObject* parent)
    : QObject(parent) {}

GlobalShortcutManager::~GlobalShortcutManager() {
    const QStringList actionIds = m_hotkeys.keys();
    for (const QString& actionId : actionIds) {
        unregisterShortcut(actionId);
    }
}

bool GlobalShortcutManager::platformSupportsHotkeys() {
    // Global hotkeys are unavailable on Wayland; QHotkey's X11 backend also
    // crashes under headless platforms (offscreen/minimal), so refuse there
    // too. Real sessions report xcb / windows / cocoa.
    const QString platform = QGuiApplication::platformName().toLower();
    return !(platform == QLatin1String("wayland") || platform == QLatin1String("offscreen") ||
             platform == QLatin1String("minimal") || platform == QLatin1String("dummy"));
}

QString GlobalShortcutManager::actionLabel(const QString& actionId) {
    // Kept in sync with the tray menu wording so the message names a feature
    // the user can find in the UI.
    if (actionId == QLatin1String("translateSelection")) {
        return QStringLiteral("Translate selection");
    }
    if (actionId == QLatin1String("translateScreenshot")) {
        return QStringLiteral("Translate screenshot");
    }
    return actionId;
}

QString GlobalShortcutManager::conflictReason(const QString& conflictingActionId) {
    // Constant sentences (the action name comes from actionLabel) so they can
    // live in the I18n table verbatim.
    if (conflictingActionId == QLatin1String("translateSelection")) {
        return QStringLiteral("This shortcut is already assigned to Translate selection");
    }
    if (conflictingActionId == QLatin1String("translateScreenshot")) {
        return QStringLiteral("This shortcut is already assigned to Translate screenshot");
    }
    return QStringLiteral("This shortcut is already assigned to another action");
}

void GlobalShortcutManager::setLastError(const QString& error) {
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

bool GlobalShortcutManager::fail(const QString& actionId, const QString& reason) {
    setLastError(reason);
    emit shortcutRegisterFailed(actionId, reason);
    return false;
}

bool GlobalShortcutManager::registerShortcut(const QString& actionId, const QString& sequence) {
    setLastError(QString());

    const QKeySequence keySequence(sequence);
    if (keySequence.isEmpty()) {
        return fail(actionId, QStringLiteral("Shortcut is empty"));
    }
    if (actionId.trimmed().isEmpty()) {
        return fail(actionId, QStringLiteral("Action id is empty"));
    }

    if (!platformSupportsHotkeys()) {
        return fail(actionId,
                    QStringLiteral("Global hotkeys are not available on %1").arg(QGuiApplication::platformName()));
    }

    // One chord, one action. Checked before the platform call because QHotkey
    // would otherwise accept the duplicate silently and fire both actions.
    const QString conflictingAction = findConflictingAction(m_sequences, actionId, sequence);
    if (!conflictingAction.isEmpty()) {
        return fail(actionId, conflictReason(conflictingAction));
    }

    // Rebinding replaces the previous registration of this same action.
    unregisterShortcut(actionId);

    const QString canonical = keySequence.toString(QKeySequence::PortableText);

    QHotkey* hotkey = new QHotkey(keySequence, true, this);
    connect(hotkey, &QHotkey::activated, this, [this, actionId]() { emit shortcutActivated(actionId); });

    if (!hotkey->isRegistered()) {
        hotkey->deleteLater();
        return fail(actionId, QStringLiteral("Failed to register shortcut: %1").arg(canonical));
    }

    m_hotkeys.insert(actionId, hotkey);
    m_sequences.insert(actionId, canonical);
    return true;
}

void GlobalShortcutManager::unregisterShortcut(const QString& actionId) {
    m_sequences.remove(actionId);

    QHotkey* hotkey = m_hotkeys.take(actionId);
    if (!hotkey) {
        return;
    }
    hotkey->setRegistered(false);
    hotkey->deleteLater();
}

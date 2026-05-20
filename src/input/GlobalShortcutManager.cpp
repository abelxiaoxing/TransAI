#include "input/GlobalShortcutManager.h"

#include <QKeySequence>

#include "input/QHotkeyShortcutBackend.h"

GlobalShortcutManager::GlobalShortcutManager(QObject* parent)
    : QObject(parent)
    , m_backend(new QHotkeyShortcutBackend(this))
{
    connect(m_backend.data(), &ShortcutBackend::activated, this, &GlobalShortcutManager::shortcutActivated);
    connect(m_backend.data(), &ShortcutBackend::failed, this, &GlobalShortcutManager::shortcutRegisterFailed);
}

bool GlobalShortcutManager::registerShortcut(const QString& actionId, const QString& sequence)
{
    const QKeySequence keySequence(sequence);
    if (keySequence.isEmpty()) {
        emit shortcutRegisterFailed(actionId, "Shortcut is empty");
        return false;
    }
    return m_backend->registerShortcut(actionId, keySequence);
}

void GlobalShortcutManager::unregisterShortcut(const QString& actionId)
{
    m_backend->unregisterShortcut(actionId);
}

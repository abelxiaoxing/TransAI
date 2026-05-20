#ifndef QHOTKEY_SHORTCUT_BACKEND_H
#define QHOTKEY_SHORTCUT_BACKEND_H

#include <QHash>
#include <QHotkey>

#include "input/ShortcutBackend.h"

class QHotkeyShortcutBackend : public ShortcutBackend
{
    Q_OBJECT

public:
    explicit QHotkeyShortcutBackend(QObject* parent = nullptr);
    ~QHotkeyShortcutBackend() override;

    bool registerShortcut(const QString& actionId, const QKeySequence& sequence) override;
    void unregisterShortcut(const QString& actionId) override;

private:
    QHash<QString, QHotkey*> m_hotkeys;
};

#endif // QHOTKEY_SHORTCUT_BACKEND_H

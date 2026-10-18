#ifndef SHORTCUTCONFLICT_H
#define SHORTCUTCONFLICT_H

#include <QHash>
#include <QKeySequence>
#include <QString>

// Global-hotkey uniqueness arbitration (header-only so it can be unit-tested
// without a display or a real hotkey backend).
//
// One chord may only drive one action. QHotkey on its own does NOT reject a
// duplicate: QHotkeyPrivate::addShortcutInvoked() skips the native grab when
// the shortcut is already in its QMultiHash and reports success, then
// activateShortcut() invokes every QHotkey bound to that chord - so a single
// key press would fire all of them. GlobalShortcutManager therefore consults
// this helper before touching the platform.
//
// `registered` maps actionId -> the shortcut string currently in effect.
// Comparison goes through QKeySequence, so "Ctrl+Shift+A", "Shift+Ctrl+A" and
// different letter cases all count as the same chord.
inline QString findConflictingAction(const QHash<QString, QString>& registered, const QString& actionId,
                                     const QString& sequence) {
    const QKeySequence candidate(sequence);
    if (candidate.isEmpty()) {
        return QString();
    }

    for (auto it = registered.cbegin(); it != registered.cend(); ++it) {
        // Re-registering the same action is a replacement, not a conflict.
        if (it.key() == actionId) {
            continue;
        }
        const QKeySequence existing(it.value());
        if (existing.isEmpty()) {
            continue;
        }
        if (existing == candidate) {
            return it.key();
        }
    }
    return QString();
}

#endif  // SHORTCUTCONFLICT_H

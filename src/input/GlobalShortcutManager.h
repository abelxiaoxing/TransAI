#ifndef GLOBAL_SHORTCUT_MANAGER_H
#define GLOBAL_SHORTCUT_MANAGER_H

#include <QHash>
#include <QObject>

#include <QHotkey>

// Registers process-wide hotkeys via QHotkey (X11 / Windows / macOS).
// On Wayland global hotkey registration is unavailable; bind a compositor
// shortcut to `TransAI --translate-selection` instead, which reaches the
// running instance through the application command server.
//
// This class is the single authority over which action owns which chord: it
// refuses to register a sequence that another action already holds (see
// input/ShortcutConflict.h for why QHotkey alone is not enough), and it keeps
// the last failure reason so the UI can show one consistent message instead of
// one ad-hoc sentence per recorder.
class GlobalShortcutManager : public QObject {
    Q_OBJECT
    // False on Wayland/headless platforms where QHotkey cannot work; the
    // settings UI uses this to explain why hotkey recording is unavailable.
    Q_PROPERTY(bool hotkeysSupported READ hotkeysSupported CONSTANT)
    // Reason of the most recent registration failure ("" after a success).
    // Fixed English sentences for the messages that are constant, so QML can
    // translate them through the I18n table.
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit GlobalShortcutManager(QObject* parent = nullptr);
    ~GlobalShortcutManager() override;

    bool hotkeysSupported() const { return platformSupportsHotkeys(); }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE bool registerShortcut(const QString& actionId, const QString& sequence);
    Q_INVOKABLE void unregisterShortcut(const QString& actionId);

signals:
    void shortcutActivated(QString actionId);
    void shortcutRegisterFailed(QString actionId, QString reason);
    void lastErrorChanged();

private:
    static bool platformSupportsHotkeys();
    // User-facing name of an action, used inside conflict messages.
    static QString actionLabel(const QString& actionId);
    // Fixed, translatable sentence for "that chord belongs to someone else".
    static QString conflictReason(const QString& conflictingActionId);
    bool fail(const QString& actionId, const QString& reason);
    void setLastError(const QString& error);

    QHash<QString, QHotkey*> m_hotkeys;
    // actionId -> sequence currently in effect (canonical PortableText form);
    // the conflict check compares against this map.
    QHash<QString, QString> m_sequences;
    QString m_lastError;
};

#endif  // GLOBAL_SHORTCUT_MANAGER_H

#ifndef GLOBAL_SHORTCUT_MANAGER_H
#define GLOBAL_SHORTCUT_MANAGER_H

#include <QHash>
#include <QObject>

#include <QHotkey>

// Registers process-wide hotkeys via QHotkey (X11 / Windows / macOS).
// On Wayland global hotkey registration is unavailable; bind a compositor
// shortcut to `TransAI --translate-selection` instead, which reaches the
// running instance through the application command server.
class GlobalShortcutManager : public QObject
{
    Q_OBJECT

public:
    explicit GlobalShortcutManager(QObject* parent = nullptr);
    ~GlobalShortcutManager() override;

    Q_INVOKABLE bool registerShortcut(const QString& actionId, const QString& sequence);
    Q_INVOKABLE void unregisterShortcut(const QString& actionId);

signals:
    void shortcutActivated(QString actionId);
    void shortcutRegisterFailed(QString actionId, QString reason);

private:
    QHash<QString, QHotkey*> m_hotkeys;
};

#endif // GLOBAL_SHORTCUT_MANAGER_H

#ifndef GLOBAL_SHORTCUT_MANAGER_H
#define GLOBAL_SHORTCUT_MANAGER_H

#include <QObject>
#include <QScopedPointer>

#include "input/ShortcutBackend.h"

class GlobalShortcutManager : public QObject
{
    Q_OBJECT

public:
    explicit GlobalShortcutManager(QObject* parent = nullptr);

    Q_INVOKABLE bool registerShortcut(const QString& actionId, const QString& sequence);
    Q_INVOKABLE void unregisterShortcut(const QString& actionId);

signals:
    void shortcutActivated(QString actionId);
    void shortcutRegisterFailed(QString actionId, QString reason);

private:
    QScopedPointer<ShortcutBackend> m_backend;
};

#endif // GLOBAL_SHORTCUT_MANAGER_H

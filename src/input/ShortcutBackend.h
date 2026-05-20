#ifndef SHORTCUT_BACKEND_H
#define SHORTCUT_BACKEND_H

#include <QObject>
#include <QKeySequence>
#include <QString>

class ShortcutBackend : public QObject
{
    Q_OBJECT

public:
    explicit ShortcutBackend(QObject* parent = nullptr) : QObject(parent) {}
    ~ShortcutBackend() override = default;

    virtual bool registerShortcut(const QString& actionId, const QKeySequence& sequence) = 0;
    virtual void unregisterShortcut(const QString& actionId) = 0;

signals:
    void activated(QString actionId);
    void failed(QString actionId, QString reason);
};

#endif // SHORTCUT_BACKEND_H

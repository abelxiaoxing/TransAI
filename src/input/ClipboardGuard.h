#ifndef CLIPBOARD_GUARD_H
#define CLIPBOARD_GUARD_H

#include <QObject>
#include <QString>

class ClipboardGuard : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardGuard(QObject* parent = nullptr);

    QString originalText() const;
    QString currentText() const;
    bool restore() const;

private:
    QString m_originalText;
};

#endif // CLIPBOARD_GUARD_H

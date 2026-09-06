#ifndef CLIPBOARD_GUARD_H
#define CLIPBOARD_GUARD_H

#include <QObject>
#include <QMap>
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
    bool m_hadMimeData = false;
    QString m_originalText;
    QMap<QString, QByteArray> m_originalMimeData;
};

#endif // CLIPBOARD_GUARD_H

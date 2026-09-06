#ifndef SELECTION_TEXT_PROVIDER_H
#define SELECTION_TEXT_PROVIDER_H

#include <QObject>

class QProcess;

class SelectionTextProvider : public QObject
{
    Q_OBJECT

public:
    explicit SelectionTextProvider(QObject* parent = nullptr);

    Q_INVOKABLE void requestSelectedText();

signals:
    void selectedTextReady(QString text);
    void selectedTextFailed(QString reason);

private:
    void requestWaylandSelectedText();
    void requestSelectedTextBySimulatedCopy();
    bool emitQtPrimarySelection();
    void failWaylandRequest(const QString& detail = QString());
    bool simulateCopy() const;

    QProcess* m_waylandSelectionProcess = nullptr;
};

#endif // SELECTION_TEXT_PROVIDER_H

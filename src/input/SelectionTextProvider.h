#ifndef SELECTION_TEXT_PROVIDER_H
#define SELECTION_TEXT_PROVIDER_H

#include <QMetaObject>
#include <QObject>

class ClipboardGuard;
class QProcess;
class QTimer;

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
    // Completes a simulated copy as soon as the clipboard actually changes;
    // the timer is only the fallback ceiling for apps that never do.
    void completeSimulatedCopy();
    void abortPendingCopy();
    bool emitQtPrimarySelection();
    void failWaylandRequest(const QString& detail = QString());
    bool simulateCopy() const;

    QProcess* m_waylandSelectionProcess = nullptr;

    // In-flight simulated-copy state (X11/Windows/macOS path).
    ClipboardGuard* m_clipboardGuard = nullptr;
    QTimer* m_clipboardWaitTimer = nullptr;
    QMetaObject::Connection m_clipboardConnection;
    QString m_copyMarker;
};

#endif // SELECTION_TEXT_PROVIDER_H

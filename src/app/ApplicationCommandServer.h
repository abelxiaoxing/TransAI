#ifndef APPLICATION_COMMAND_SERVER_H
#define APPLICATION_COMMAND_SERVER_H

#include <QHash>
#include <QLocalServer>
#include <QObject>

class QLocalSocket;

class ApplicationCommandServer : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationCommandServer(QObject* parent = nullptr);
    ~ApplicationCommandServer() override;

    // Forwards `command` to an already running instance. Returns false when
    // nobody accepted it (no live instance, pipe/socket refused the write, or
    // the write did not complete in time); `error` receives a human-readable
    // reason that is meant to end up in the log, since the failure path is
    // otherwise silent.
    static bool sendCommandToRunningInstance(const QString& command, int timeoutMs = 500,
                                             QString* error = nullptr);

    bool start();
    QString errorString() const;

signals:
    void commandReceived(QString command);

private:
    static QString serverName();
    // True when something is already listening on `name` (used to refuse to
    // steal a live instance's socket, and to detect stale leftovers).
    static bool hasLiveInstance(const QString& name);
    void acceptConnections();
    void readFromClient(QLocalSocket* socket);
    void finishClient(QLocalSocket* socket);

    QLocalServer m_server;
    QHash<QLocalSocket*, QByteArray> m_buffers;
    // Set when start() fails for a reason QLocalServer does not report itself.
    QString m_errorString;
};

#endif // APPLICATION_COMMAND_SERVER_H

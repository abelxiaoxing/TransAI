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

    static bool sendCommandToRunningInstance(const QString& command, int timeoutMs = 500);

    bool start();
    QString errorString() const;

signals:
    void commandReceived(QString command);

private:
    static QString serverName();
    void acceptConnections();
    void readFromClient(QLocalSocket* socket);
    void finishClient(QLocalSocket* socket);

    QLocalServer m_server;
    QHash<QLocalSocket*, QByteArray> m_buffers;
};

#endif // APPLICATION_COMMAND_SERVER_H

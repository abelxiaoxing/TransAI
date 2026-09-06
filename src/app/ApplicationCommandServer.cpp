#include "app/ApplicationCommandServer.h"

#include <QDir>
#include <QLocalSocket>
#include <QStandardPaths>

namespace {
constexpr auto kServerBaseName = "transai-command";
constexpr qsizetype kMaximumCommandBytes = 4096;
}

ApplicationCommandServer::ApplicationCommandServer(QObject* parent)
    : QObject(parent)
{
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_server, &QLocalServer::newConnection, this, &ApplicationCommandServer::acceptConnections);
}

bool ApplicationCommandServer::sendCommandToRunningInstance(const QString& command, int timeoutMs)
{
    if (command.trimmed().isEmpty()) {
        return false;
    }

    QLocalSocket socket;
    socket.connectToServer(serverName(), QIODevice::WriteOnly);
    if (!socket.waitForConnected(timeoutMs)) {
        return false;
    }

    const QByteArray payload = command.trimmed().toUtf8() + '\n';
    if (socket.write(payload) != payload.size()) {
        return false;
    }
    if (!socket.waitForBytesWritten(timeoutMs)) {
        return false;
    }

    socket.disconnectFromServer();
    return true;
}

bool ApplicationCommandServer::start()
{
    const QString name = serverName();
    if (m_server.listen(name)) {
        return true;
    }

    if (m_server.serverError() != QAbstractSocket::AddressInUseError) {
        return false;
    }

    // A crashed process can leave a filesystem socket behind. Never remove it
    // until a connection attempt has proved that no live instance owns it.
    QLocalSocket probe;
    probe.connectToServer(name, QIODevice::WriteOnly);
    if (probe.waitForConnected(200)) {
        probe.disconnectFromServer();
        return false;
    }

    if (!QLocalServer::removeServer(name)) {
        return false;
    }
    return m_server.listen(name);
}

QString ApplicationCommandServer::errorString() const
{
    return m_server.errorString();
}

QString ApplicationCommandServer::serverName()
{
#ifdef Q_OS_UNIX
    const QString runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (!runtimeDirectory.isEmpty()) {
        return QDir(runtimeDirectory).filePath(kServerBaseName);
    }
#endif
    return QString::fromLatin1(kServerBaseName);
}

void ApplicationCommandServer::acceptConnections()
{
    while (QLocalSocket* socket = m_server.nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            readFromClient(socket);
        });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket]() {
            finishClient(socket);
        });
    }
}

void ApplicationCommandServer::readFromClient(QLocalSocket* socket)
{
    if (!socket || !m_buffers.contains(socket)) {
        return;
    }

    QByteArray& buffer = m_buffers[socket];
    buffer += socket->readAll();
    if (buffer.size() > kMaximumCommandBytes) {
        buffer.clear();
        socket->disconnectFromServer();
        return;
    }

    qsizetype newline = -1;
    while ((newline = buffer.indexOf('\n')) >= 0) {
        const QString command = QString::fromUtf8(buffer.first(newline)).trimmed();
        buffer.remove(0, newline + 1);
        if (!command.isEmpty()) {
            emit commandReceived(command);
        }
    }
}

void ApplicationCommandServer::finishClient(QLocalSocket* socket)
{
    if (!socket) {
        return;
    }

    readFromClient(socket);
    const QString trailingCommand = QString::fromUtf8(m_buffers.take(socket)).trimmed();
    if (!trailingCommand.isEmpty()) {
        emit commandReceived(trailingCommand);
    }
    socket->deleteLater();
}

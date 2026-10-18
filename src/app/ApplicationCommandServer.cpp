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

    // Refuse to take the socket away from a live instance. Depending on the Qt
    // version, listen() may silently unlink and rebind an existing socket file,
    // so the check cannot be deferred to listen()'s AddressInUseError. (The
    // primary guard is still sendCommandToRunningInstance() in main(); this
    // covers the startup race.)
    if (hasLiveInstance(name)) {
        m_errorString = QStringLiteral("Another TransAI instance is already running");
        return false;
    }

    if (m_server.listen(name)) {
        return true;
    }

    if (m_server.serverError() != QAbstractSocket::AddressInUseError) {
        m_errorString = m_server.errorString();
        return false;
    }

    // A crashed process can leave a filesystem socket behind. Only remove it
    // once a connection attempt has proved that no live instance owns it.
    if (!QLocalServer::removeServer(name)) {
        m_errorString = m_server.errorString();
        return false;
    }
    if (!m_server.listen(name)) {
        m_errorString = m_server.errorString();
        return false;
    }
    return true;
}

QString ApplicationCommandServer::errorString() const
{
    return m_errorString.isEmpty() ? m_server.errorString() : m_errorString;
}

bool ApplicationCommandServer::hasLiveInstance(const QString& name)
{
    QLocalSocket probe;
    probe.connectToServer(name, QIODevice::WriteOnly);
    if (!probe.waitForConnected(200)) {
        return false;
    }
    probe.disconnectFromServer();
    return true;
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

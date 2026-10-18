#include "app/ApplicationCommandServer.h"

#include <QCoreApplication>
#include <QDir>
#include <QStringList>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QThread>

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

ApplicationCommandServer::~ApplicationCommandServer()
{
    // nextPendingConnection() parents the sockets to this object, so QObject's
    // destructor deletes them — and QLocalSocket's destructor calls
    // disconnectFromHost(), which emits disconnected() *synchronously*. That
    // reaches finishClient() while m_buffers has already been destroyed
    // (members are destroyed before the QObject base). Detach the sockets first
    // so no handler can run against a half-destroyed server.
    const QList<QLocalSocket*> sockets = findChildren<QLocalSocket*>();
    for (QLocalSocket* socket : sockets) {
        disconnect(socket, nullptr, this, nullptr);
        delete socket;
    }
    m_buffers.clear();
}

bool ApplicationCommandServer::sendCommandToRunningInstance(const QString& command, int timeoutMs,
                                                           QString* error)
{
    auto fail = [error](const QString& reason) {
        if (error) {
            *error = reason;
        }
        return false;
    };

    if (command.trimmed().isEmpty()) {
        return fail(QStringLiteral("the command is empty"));
    }

    const QString name = serverName();
    QLocalSocket socket;
    socket.connectToServer(name, QIODevice::WriteOnly);
    if (!socket.waitForConnected(timeoutMs)) {
        return fail(QStringLiteral("could not connect to %1 within %2 ms: %3")
                        .arg(name)
                        .arg(timeoutMs)
                        .arg(socket.errorString()));
    }

    const QByteArray payload = command.trimmed().toUtf8() + '\n';
    if (socket.write(payload) != payload.size()) {
        return fail(QStringLiteral("could not write to %1: %2").arg(name, socket.errorString()));
    }
    // Flush with the event loop running instead of waitForBytesWritten(): on
    // Windows the completion of the overlapped named-pipe write is delivered
    // through the thread's event dispatcher, so a bare waitForBytesWritten()
    // times out whenever the peer has not pumped its own event loop yet (a
    // second instance launched right after the first, or a server living in the
    // same thread). Unix domain sockets complete synchronously, which is why
    // only Windows ever hit this.
    QElapsedTimer timer;
    timer.start();
    while (socket.bytesToWrite() > 0 && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        if (socket.bytesToWrite() > 0) {
            QThread::msleep(1);
        }
    }
    if (socket.bytesToWrite() > 0) {
        return fail(QStringLiteral("the write to %1 did not complete within %2 ms").arg(name).arg(timeoutMs));
    }

    socket.disconnectFromServer();
    if (error) {
        error->clear();
    }
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

    const auto it = m_buffers.find(socket);
    if (it == m_buffers.end()) {
        return;
    }

    QByteArray& buffer = it.value();
    buffer += socket->readAll();
    if (buffer.size() > kMaximumCommandBytes) {
        buffer.clear();
        socket->disconnectFromServer();
        return;
    }

    // Collect first, emit afterwards: a slot connected to commandReceived may
    // disconnect the client, which re-enters finishClient() and removes this
    // entry from m_buffers — touching `buffer` after that would be a
    // use-after-free.
    QStringList commands;
    qsizetype newline = -1;
    while ((newline = buffer.indexOf('\n')) >= 0) {
        const QString command = QString::fromUtf8(buffer.first(newline)).trimmed();
        buffer.remove(0, newline + 1);
        if (!command.isEmpty()) {
            commands << command;
        }
    }
    for (const QString& command : commands) {
        emit commandReceived(command);
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

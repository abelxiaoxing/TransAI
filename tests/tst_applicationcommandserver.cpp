#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "app/ApplicationCommandServer.h"

// Single-instance plumbing: command forwarding to the running instance and
// recovery from a socket file left behind by a crashed process (the "startup
// does nothing" failure mode users hit after a kill -9).
//
// XDG_RUNTIME_DIR is pointed at a private temp directory in main() so the
// socket path is deterministic and never collides with a real running app.
namespace {
constexpr auto kSocketName = "transai-command";
}

class TestApplicationCommandServer : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void forwardsCommandToRunningInstance();
    void staleSocketArtifactIsReclaimed();
    void emptyCommandIsRejected();
    void secondInstanceCannotStealLiveSocket();

private:
    static QString socketPath() {
        const QString runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (!runtimeDirectory.isEmpty()) {
            return QDir(runtimeDirectory).filePath(QLatin1String(kSocketName));
        }
        return QString::fromLatin1(kSocketName);
    }
};

void TestApplicationCommandServer::init() {
    QFile::remove(socketPath());
}

void TestApplicationCommandServer::cleanup() {
    QFile::remove(socketPath());
}

void TestApplicationCommandServer::forwardsCommandToRunningInstance() {
    ApplicationCommandServer server;
    QVERIFY(server.start());

    QSignalSpy receivedSpy(&server, &ApplicationCommandServer::commandReceived);
#ifdef Q_OS_WIN
    // Known Windows gap: listen() succeeds, but connectToServer() against the
    // resulting named pipe fails, so single-instance command forwarding is
    // unverified there. Marked expected-failure (not skipped) so the day it
    // starts working CI reports an unexpected pass instead of silently keeping
    // a stale TODO.
    QEXPECT_FAIL("", "QLocalServer forwarding is not verified on Windows yet", Abort);
#endif
    QVERIFY(ApplicationCommandServer::sendCommandToRunningInstance(QStringLiteral("translate-selection")));

    QTRY_COMPARE(receivedSpy.count(), 1);
    QCOMPARE(receivedSpy.takeFirst().at(0).toString(), QStringLiteral("translate-selection"));
}

void TestApplicationCommandServer::staleSocketArtifactIsReclaimed() {
#ifdef Q_OS_WIN
    QEXPECT_FAIL("", "QLocalServer forwarding is not verified on Windows yet", Abort);
#endif
    QVERIFY(!QFile::exists(socketPath()));

    // A crashed instance leaves the socket file behind; the bytes are
    // irrelevant, only that the path exists and nothing is listening.
    {
        QFile stale(socketPath());
        QVERIFY(stale.open(QIODevice::WriteOnly | QIODevice::Truncate));
        stale.write("stale");
    }

    ApplicationCommandServer server;
    QVERIFY2(server.start(), qPrintable(server.errorString()));

    QSignalSpy receivedSpy(&server, &ApplicationCommandServer::commandReceived);
    QVERIFY(ApplicationCommandServer::sendCommandToRunningInstance(QStringLiteral("show")));
    QTRY_COMPARE(receivedSpy.count(), 1);
}

void TestApplicationCommandServer::emptyCommandIsRejected() {
    ApplicationCommandServer server;
    QVERIFY(server.start());

    // Sending nothing would be a no-op for the receiver; refuse it locally
    // instead of opening a connection.
    QVERIFY(!ApplicationCommandServer::sendCommandToRunningInstance(QString()));
    QVERIFY(!ApplicationCommandServer::sendCommandToRunningInstance(QStringLiteral("   ")));
}

void TestApplicationCommandServer::secondInstanceCannotStealLiveSocket() {
    ApplicationCommandServer first;
    QVERIFY(first.start());

    // A second server must refuse to take over a socket that still answers.
    ApplicationCommandServer second;
    QVERIFY(!second.start());
    QVERIFY(!second.errorString().isEmpty());
}

int main(int argc, char* argv[]) {
    // Deterministic, private socket path: RuntimeLocation uses XDG_RUNTIME_DIR
    // on Unix and falls back to the temp dir elsewhere.
    QTemporaryDir runtimeDir;
    if (runtimeDir.isValid()) {
        qputenv("XDG_RUNTIME_DIR", runtimeDir.path().toUtf8());
    }
    QCoreApplication app(argc, argv);

    TestApplicationCommandServer test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_applicationcommandserver.moc"

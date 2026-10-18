#include <QNetworkReply>
#include <QtTest>

#include "providers/NetworkErrorMapper.h"

// Locks down the shared HTTP/network error mapping. The translation pipeline
// and model detection both use it, so a regression here would change
// user-facing wording in two places at once.
class TestNetworkErrorMapper : public QObject {
    Q_OBJECT

private slots:
    void mapsTransportErrors();
    void mapsProxyErrors();
    void mapsContentErrors();
    void mapsProtocolErrors();
    void mapsServerErrors();
    void unknownCodesFallBackToGeneric();
};

void TestNetworkErrorMapper::mapsTransportErrors() {
    QCOMPARE(NetworkErrorMapper::messageFor(QNetworkReply::ConnectionRefusedError),
             QStringLiteral("Network error: could not reach the server"));
    QCOMPARE(NetworkErrorMapper::messageFor(QNetworkReply::HostNotFoundError),
             QStringLiteral("Network error: could not reach the server"));
    QCOMPARE(NetworkErrorMapper::messageFor(QNetworkReply::OperationCanceledError),
             QStringLiteral("Network error: could not reach the server"));
}

void TestNetworkErrorMapper::mapsProxyErrors() {
    QCOMPARE(NetworkErrorMapper::messageFor(QNetworkReply::ProxyConnectionRefusedError),
             QStringLiteral("Proxy error: could not reach the server through the proxy"));
    QCOMPARE(NetworkErrorMapper::messageFor(QNetworkReply::ProxyAuthenticationRequiredError),
             QStringLiteral("Proxy error: could not reach the server through the proxy"));
}

void TestNetworkErrorMapper::mapsContentErrors() {
    // HTTP 401/403/404 land in this range.
    QCOMPARE(NetworkErrorMapper::messageFor(QNetworkReply::ContentAccessDenied),
             QStringLiteral("Request rejected: check the API key, model name and server URL"));
    QCOMPARE(NetworkErrorMapper::messageFor(QNetworkReply::ContentNotFoundError),
             QStringLiteral("Request rejected: check the API key, model name and server URL"));
}

void TestNetworkErrorMapper::mapsProtocolErrors() {
    QCOMPARE(NetworkErrorMapper::messageFor(QNetworkReply::ProtocolFailure),
             QStringLiteral("Protocol error: unexpected response from the server"));
}

void TestNetworkErrorMapper::mapsServerErrors() {
    QCOMPARE(NetworkErrorMapper::messageFor(QNetworkReply::InternalServerError),
             QStringLiteral("Server error: the API server failed to handle the request"));
    QCOMPARE(NetworkErrorMapper::messageFor(QNetworkReply::UnknownServerError),
             QStringLiteral("Server error: the API server failed to handle the request"));
}

void TestNetworkErrorMapper::unknownCodesFallBackToGeneric() {
    // NoError (0) is not an error at all; anything outside the documented
    // ranges must still produce a user-presentable message.
    QCOMPARE(NetworkErrorMapper::messageFor(QNetworkReply::NoError), QStringLiteral("Network error"));
}

QTEST_GUILESS_MAIN(TestNetworkErrorMapper)
#include "tst_networkerrormapper.moc"

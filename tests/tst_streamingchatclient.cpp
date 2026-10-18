#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>

#include <QImage>

#include "translation/TranslationService.h"
#include "vision/VisionTranslationService.h"

// Exercises StreamingChatClient end-to-end through its TranslationService
// subclass against a scripted in-process HTTP server: SSE streaming, the
// thinking-params rejection retry, error mapping and abort semantics.

namespace {

QByteArray sseDelta(const QString& content)
{
    return "data: {\"choices\":[{\"delta\":{\"content\":\"" + content.toUtf8() + "\"}}]}\n\n";
}

struct ScriptedResponse {
    int statusCode = 200;
    QByteArray reason = "OK";
    QByteArray contentType = "text/event-stream";
    QByteArray body;
    // Never respond (used to keep a request in flight for the abort test).
    bool hang = false;
};

// Minimal HTTP/1.1 server: reads one request per connection (headers +
// Content-Length body), records the body, then plays back the next scripted
// response. Responses are sent with "Connection: close" so every request is
// its own connection.
class FakeChatServer : public QObject
{
    Q_OBJECT
public:
    QList<ScriptedResponse> responses;
    QList<QByteArray> receivedBodies;

    bool start()
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket* socket = m_server.nextPendingConnection();
            socket->setParent(this);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { feed(socket); });
            connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
        });
        return m_server.listen(QHostAddress::LocalHost, 0);
    }

    int port() const { return static_cast<int>(m_server.serverPort()); }

private:
    void feed(QTcpSocket* socket)
    {
        QByteArray& buffer = m_buffers[socket];
        buffer += socket->readAll();

        const int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return;
        }
        const QByteArray header = buffer.left(headerEnd);

        int contentLength = 0;
        const QList<QByteArray> lines = header.split('\n');
        for (const QByteArray& line : lines) {
            const QByteArray trimmed = line.trimmed();
            if (trimmed.toLower().startsWith("content-length:")) {
                contentLength = trimmed.mid(15).trimmed().toInt();
            }
        }
        if (buffer.size() < headerEnd + 4 + contentLength) {
            return; // body not fully arrived yet
        }

        const QByteArray requestBody = buffer.mid(headerEnd + 4, contentLength);
        const int index = static_cast<int>(receivedBodies.size());
        receivedBodies.push_back(requestBody);
        m_buffers.remove(socket);

        if (index >= responses.size()) {
            socket->disconnectFromHost();
            return;
        }
        const ScriptedResponse& response = responses.at(index);
        if (response.hang) {
            return; // keep the connection open without answering
        }

        QByteArray head = "HTTP/1.1 " + QByteArray::number(response.statusCode) + " " + response.reason
            + "\r\nContent-Type: " + response.contentType
            + "\r\nContent-Length: " + QByteArray::number(response.body.size())
            + "\r\nConnection: close\r\n\r\n";
        socket->write(head + response.body);
        socket->flush();
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_buffers;
};

void translateOllama(TranslationService& service, const QString& apiServer,
                     const QString& text = QStringLiteral("hello"))
{
    service.translate(text, QStringLiteral("English"), QStringLiteral("ollama"),
                      apiServer, QString(), QStringLiteral("qwen3"));
}

} // namespace

class TestStreamingChatClient : public QObject
{
    Q_OBJECT

private slots:
    void streamsContentToFinished();
    void retriesWithoutThinkingParamsOnRejection();
    void surfacesServerErrorPayload();
    void mapsUnparseableServerError();
    void abortEmitsStopped();
    void rejectsEmptyTextWithoutRequest();
    void visionRequestBodyCarriesImageAndPrompt();
};

void TestStreamingChatClient::streamsContentToFinished()
{
    FakeChatServer server;
    QVERIFY(server.start());
    server.responses << ScriptedResponse{ 200, "OK", "text/event-stream",
        sseDelta(QStringLiteral("Hello")) + sseDelta(QStringLiteral(" world")) + "data: [DONE]\n\n", false };

    TranslationService service;
    QSignalSpy startedSpy(&service, &TranslationService::translationStarted);
    QSignalSpy finishedSpy(&service, &TranslationService::translationFinished);
    QSignalSpy failedSpy(&service, &TranslationService::translationFailed);

    translateOllama(service, QStringLiteral("http://127.0.0.1:%1").arg(server.port()));

    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() == 1, 5000);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(finishedSpy.takeFirst().at(0).toString(), QStringLiteral("Hello world"));
    QCOMPARE(server.receivedBodies.size(), 1);
    QVERIFY(!service.isRequesting());
}

void TestStreamingChatClient::retriesWithoutThinkingParamsOnRejection()
{
    FakeChatServer server;
    QVERIFY(server.start());
    // The Ollama policy always adds {"think": false}; the fake endpoint is a
    // strict relay that rejects unknown fields, then accepts the retry.
    server.responses << ScriptedResponse{ 400, "Bad Request", "application/json",
        "{\"error\":{\"message\":\"unknown parameter: think\"}}", false }
                     << ScriptedResponse{ 200, "OK", "text/event-stream",
        sseDelta(QStringLiteral("Hi")) + "data: [DONE]\n\n", false };

    TranslationService service;
    QSignalSpy startedSpy(&service, &TranslationService::translationStarted);
    QSignalSpy finishedSpy(&service, &TranslationService::translationFinished);
    QSignalSpy failedSpy(&service, &TranslationService::translationFailed);

    translateOllama(service, QStringLiteral("http://127.0.0.1:%1").arg(server.port()));

    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() == 1, 5000);
    QCOMPARE(failedSpy.count(), 0);
    // Exactly one user-facing request: started fires once despite the resend.
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(finishedSpy.takeFirst().at(0).toString(), QStringLiteral("Hi"));

    QCOMPARE(server.receivedBodies.size(), 2);
    const QJsonObject firstBody = QJsonDocument::fromJson(server.receivedBodies.at(0)).object();
    const QJsonObject retryBody = QJsonDocument::fromJson(server.receivedBodies.at(1)).object();
    QVERIFY(firstBody.contains(QStringLiteral("think")));
    QVERIFY(!retryBody.contains(QStringLiteral("think")));
    // Everything else (messages included) is identical between the attempts.
    QCOMPARE(retryBody.value(QStringLiteral("messages")), firstBody.value(QStringLiteral("messages")));
}

void TestStreamingChatClient::surfacesServerErrorPayload()
{
    FakeChatServer server;
    QVERIFY(server.start());
    server.responses << ScriptedResponse{ 500, "Internal Server Error", "application/json",
        "{\"error\":{\"message\":\"backend exploded\"}}", false };

    TranslationService service;
    QSignalSpy finishedSpy(&service, &TranslationService::translationFinished);
    QSignalSpy failedSpy(&service, &TranslationService::translationFailed);

    translateOllama(service, QStringLiteral("http://127.0.0.1:%1").arg(server.port()));

    QTRY_VERIFY_WITH_TIMEOUT(failedSpy.count() == 1, 5000);
    QCOMPARE(finishedSpy.count(), 0);
    // The server's own payload message wins over the generic mapping.
    QCOMPARE(failedSpy.takeFirst().at(0).toString(), QStringLiteral("backend exploded"));
    QVERIFY(!service.isRequesting());
}

void TestStreamingChatClient::mapsUnparseableServerError()
{
    FakeChatServer server;
    QVERIFY(server.start());
    server.responses << ScriptedResponse{ 500, "Internal Server Error", "text/html",
        "<html>oops</html>", false };

    TranslationService service;
    QSignalSpy failedSpy(&service, &TranslationService::translationFailed);

    translateOllama(service, QStringLiteral("http://127.0.0.1:%1").arg(server.port()));

    QTRY_VERIFY_WITH_TIMEOUT(failedSpy.count() == 1, 5000);
    QCOMPARE(failedSpy.takeFirst().at(0).toString(),
             QStringLiteral("Server error: the API server failed to handle the request"));
}

void TestStreamingChatClient::abortEmitsStopped()
{
    FakeChatServer server;
    QVERIFY(server.start());
    server.responses << ScriptedResponse{ 200, "OK", "text/event-stream", QByteArray(), /*hang=*/true };

    TranslationService service;
    QSignalSpy finishedSpy(&service, &TranslationService::translationFinished);
    QSignalSpy failedSpy(&service, &TranslationService::translationFailed);
    QSignalSpy stoppedSpy(&service, &TranslationService::translationStopped);

    translateOllama(service, QStringLiteral("http://127.0.0.1:%1").arg(server.port()));
    QTRY_VERIFY_WITH_TIMEOUT(server.receivedBodies.size() == 1, 5000);
    QVERIFY(service.isRequesting());

    service.abort();
    QTRY_VERIFY_WITH_TIMEOUT(stoppedSpy.count() == 1, 5000);
    QVERIFY(!service.isRequesting());
    QTest::qWait(200); // no terminal signal may follow the stop
    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 0);
}

void TestStreamingChatClient::rejectsEmptyTextWithoutRequest()
{
    FakeChatServer server;
    QVERIFY(server.start());

    TranslationService service;
    QSignalSpy startedSpy(&service, &TranslationService::translationStarted);
    QSignalSpy failedSpy(&service, &TranslationService::translationFailed);

    translateOllama(service, QStringLiteral("http://127.0.0.1:%1").arg(server.port()),
                    QStringLiteral("   "));

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(failedSpy.takeFirst().at(0).toString(), QStringLiteral("Please enter text to translate"));
    QTest::qWait(200);
    QCOMPARE(server.receivedBodies.size(), 0);
}

void TestStreamingChatClient::visionRequestBodyCarriesImageAndPrompt()
{
    FakeChatServer server;
    QVERIFY(server.start());
    server.responses << ScriptedResponse{ 200, "OK", "text/event-stream",
        sseDelta(QStringLiteral("Hallo")) + "data: [DONE]\n\n", false };

    QImage image(64, 48, QImage::Format_RGB32);
    image.fill(Qt::darkGreen);

    VisionTranslationService service;
    QSignalSpy finishedSpy(&service, &VisionTranslationService::translationFinished);
    QSignalSpy failedSpy(&service, &VisionTranslationService::translationFailed);

    service.translateImage(image, QStringLiteral("English"), QStringLiteral("ollama"),
                           QStringLiteral("http://127.0.0.1:%1").arg(server.port()),
                           QString(), QStringLiteral("llava"), 1600, 85);

    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() == 1, 5000);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(server.receivedBodies.size(), 1);

    // The multimodal body is assembled in VisionTranslationService, not in
    // PromptBuilder: lock down the parts the API contract depends on.
    const QJsonObject body = QJsonDocument::fromJson(server.receivedBodies.at(0)).object();
    const QJsonArray messages = body.value(QStringLiteral("messages")).toArray();
    QVERIFY(messages.size() >= 2);

    const QJsonObject systemMessage = messages.at(0).toObject();
    QCOMPARE(systemMessage.value(QStringLiteral("role")).toString(), QStringLiteral("system"));
    QVERIFY(systemMessage.value(QStringLiteral("content")).toString()
                .contains(QStringLiteral("Extract the text from the image")));

    const QJsonArray userContent = messages.at(1).toObject().value(QStringLiteral("content")).toArray();
    bool foundImagePart = false;
    for (const QJsonValue& part : userContent) {
        const QJsonObject object = part.toObject();
        if (object.value(QStringLiteral("type")).toString() != QStringLiteral("image_url")) {
            continue;
        }
        foundImagePart = true;
        const QString url = object.value(QStringLiteral("image_url")).toObject()
                                .value(QStringLiteral("url")).toString();
        QVERIFY(url.startsWith(QStringLiteral("data:image/jpeg;base64,")));
        QVERIFY(url.size() > 100); // payload really attached
    }
    QVERIFY(foundImagePart);
    QCOMPARE(body.value(QStringLiteral("model")).toString(), QStringLiteral("llava"));
}

QTEST_GUILESS_MAIN(TestStreamingChatClient)
#include "tst_streamingchatclient.moc"

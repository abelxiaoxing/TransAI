#include "translation/StreamingChatClient.h"

#include <QJsonDocument>
#include <QTimer>

#include "providers/ThinkingPolicy.h"

namespace {
// Inactivity (transfer) timeout: a request that sees no data for 30s is
// aborted so the UI never gets stuck in the requesting state. The timeout
// resets whenever data keeps streaming.
constexpr int kInactivityTimeoutMs = 30 * 1000;
// Absolute cap per request. Slow local models can legitimately stream for
// minutes; 5 minutes is generous enough for them while still guaranteeing
// the session cannot be held forever.
constexpr int kOverallTimeoutMs = 5 * 60 * 1000;
}

StreamingChatClient::StreamingChatClient(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_overallTimeout(new QTimer(this))
{
    m_networkManager->setTransferTimeout(kInactivityTimeoutMs);
    m_overallTimeout->setSingleShot(true);
    m_overallTimeout->setInterval(kOverallTimeoutMs);
    connect(m_overallTimeout, &QTimer::timeout, this, [this]() {
        if (!m_reply) {
            return;
        }
        // Record the reason, then abort WITHOUT disconnecting: the finished
        // handler sees OperationCanceledError, prefers the recorded error and
        // emits translationFailed (a user abort() disconnects first, so the
        // two paths never mix).
        setResponseError(QStringLiteral("Translation timed out: the server took too long to respond"));
        m_reply->abort();
    });
}

QString StreamingChatClient::responseData() const
{
    return m_responseData;
}

QString StreamingChatClient::responseError() const
{
    return m_responseError;
}

bool StreamingChatClient::isRequesting() const
{
    return m_isRequesting;
}

void StreamingChatClient::abort()
{
    if (!m_reply) {
        return;
    }
    cancelCurrentRequest();
    emit translationStopped();
}

void StreamingChatClient::startRequest(const PendingRequest& request)
{
    cancelCurrentRequest();

    setResponseError("");
    setResponseData("");
    // Fresh retry budget per user-facing request.
    m_thinkingRetryUsed = false;

    m_pendingRequest = request;
    m_streamParser.reset();
    setIsRequesting(true);
    emit translationStarted();

    sendPendingRequest();
}

void StreamingChatClient::failValidation(const QString& message)
{
    // Any previous request was already cancelled by the caller's entry path
    // (startRequest cancels too), so isRequesting is false here: just record
    // the error and emit the terminal signal.
    cancelCurrentRequest();
    setResponseError("");
    setResponseData("");
    setResponseError(message);
    emit translationFailed(m_responseError);
}

QString StreamingChatClient::decorateFailureMessage(const QString& error) const
{
    return error;
}

void StreamingChatClient::sendPendingRequest()
{
    const QByteArray payload = QJsonDocument(m_pendingRequest.body).toJson();
    m_reply = m_networkManager->post(m_pendingRequest.networkRequest, payload);
    QNetworkReply* currentReply = m_reply;
    m_overallTimeout->start();

    connect(currentReply, &QNetworkReply::readyRead, this, [this, currentReply]() {
        handleStreamData(currentReply);
    });
    connect(currentReply, &QNetworkReply::finished, this, [this, currentReply]() {
        if (m_reply != currentReply) {
            currentReply->deleteLater();
            return;
        }
        m_overallTimeout->stop();

        // Final read plus the buffered tail, merged into a single result.
        const StreamParseResult result = m_streamParser.consume(currentReply->readAll());
        if (!result.content.isEmpty()) {
            setResponseData(m_responseData + result.content);
        }
        if (result.hasError && !result.errorMessage.isEmpty()) {
            setResponseError(result.errorMessage);
        }

        // When a strict endpoint rejects the automatic thinking fields as
        // unknown request parameters (HTTP 400 "extra fields not permitted",
        // "unknown parameter", ...), retry once without them before failing.
        // The empty-content guard keeps the retry to clean rejections, so a
        // response that already streamed is never replayed.
        const bool canRetryWithoutThinking = !m_thinkingRetryUsed
            && m_pendingRequest.bodyWithoutThinking != m_pendingRequest.body;
        if (canRetryWithoutThinking && m_responseData.isEmpty() && !m_responseError.isEmpty()
            && ThinkingPolicyBuilder::isThinkingParamRejection(m_responseError)) {
            m_thinkingRetryUsed = true;
            m_pendingRequest.body = m_pendingRequest.bodyWithoutThinking;
            m_streamParser.reset();
            setResponseError("");
            currentReply->deleteLater();
            m_reply = nullptr;
            sendPendingRequest();
            return;
        }

        setIsRequesting(false);

        QString terminalError;
        bool succeeded = false;
        if (currentReply->error() == QNetworkReply::NoError) {
            if (m_responseError.isEmpty()) {
                succeeded = true;
            } else {
                terminalError = m_responseError;
            }
        } else if (currentReply->error() == QNetworkReply::OperationCanceledError) {
            // abort() disconnects the reply before cancelling it, so a
            // canceled reply that still reaches finished is either the 30s
            // inactivity timeout or the overall timeout — both have (or get)
            // a recorded error message.
            terminalError = m_responseError.isEmpty()
                ? QStringLiteral("Translation timed out: no data from the server")
                : m_responseError;
        } else {
            terminalError = m_responseError.isEmpty()
                ? networkErrorMessage(currentReply->error())
                : m_responseError;
        }

        if (succeeded) {
            emit translationFinished(m_responseData);
        } else {
            terminalError = decorateFailureMessage(terminalError);
            setResponseError(terminalError);
            emit translationFailed(terminalError);
        }

        currentReply->deleteLater();
        m_reply = nullptr;
    });
}

void StreamingChatClient::cancelCurrentRequest()
{
    m_overallTimeout->stop();
    if (!m_reply) {
        return;
    }

    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
    m_streamParser.reset();
    setIsRequesting(false);
}

void StreamingChatClient::handleStreamData(QNetworkReply* reply)
{
    if (!reply || m_reply != reply) {
        return;
    }

    const StreamParseResult result = m_streamParser.appendChunk(reply->readAll());
    if (!result.content.isEmpty()) {
        setResponseData(m_responseData + result.content);
    }
    if (result.hasError && !result.errorMessage.isEmpty()) {
        // Record the error but do not emit translationFailed here; the
        // finished handler emits exactly one terminal signal per request.
        setResponseError(result.errorMessage);
    }
}

void StreamingChatClient::setResponseData(const QString& responseData)
{
    if (m_responseData == responseData) {
        return;
    }
    m_responseData = responseData;
    emit responseDataChanged();
}

void StreamingChatClient::setResponseError(const QString& responseError)
{
    if (m_responseError == responseError) {
        return;
    }
    m_responseError = responseError;
    emit responseErrorChanged();
}

void StreamingChatClient::setIsRequesting(bool isRequesting)
{
    if (m_isRequesting == isRequesting) {
        return;
    }
    m_isRequesting = isRequesting;
    emit isRequestingChanged();
}

QString StreamingChatClient::networkErrorMessage(QNetworkReply::NetworkError error)
{
    // QNetworkReply error-code ranges (see QNetworkReply documentation):
    //   1-99 network, 101-199 proxy, 201-299 content (covers HTTP 401/403/404),
    //   301-399 protocol, 401-499 server-side.
    if (error > 0 && error < 100) {
        return QStringLiteral("Network error: could not reach the server");
    }
    if (error > 100 && error < 200) {
        return QStringLiteral("Proxy error: could not reach the server through the proxy");
    }
    if (error > 200 && error < 300) {
        return QStringLiteral("Request rejected: check the API key, model name and server URL");
    }
    if (error > 300 && error < 400) {
        return QStringLiteral("Protocol error: unexpected response from the server");
    }
    if (error > 400 && error < 500) {
        return QStringLiteral("Server error: the API server failed to handle the request");
    }
    return QStringLiteral("Network error");
}

#include "providers/NetworkErrorMapper.h"

QString NetworkErrorMapper::messageFor(QNetworkReply::NetworkError error) {
    // QNetworkReply error-code ranges (see the QNetworkReply documentation):
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

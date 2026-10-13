#ifndef API_ERROR_H
#define API_ERROR_H

#include <QJsonValue>
#include <QString>

// Shared helpers for OpenAI-compatible API error payloads, used by both the
// streaming translation parser and the model discovery service.
namespace ApiError {

// Extracts a human-readable message from an error JSON value of either
// documented shape: {"error": {"message": "..."}} or {"error": "..."}.
// Returns `fallback` when the value is absent or has neither shape.
inline QString messageFromValue(const QJsonValue& errorValue, const QString& fallback)
{
    if (errorValue.isObject()) {
        return errorValue.toObject().value("message").toString(fallback);
    }
    return errorValue.toString(fallback);
}

} // namespace ApiError

#endif // API_ERROR_H

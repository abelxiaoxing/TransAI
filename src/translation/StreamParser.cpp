#include "translation/StreamParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

void StreamParser::reset()
{
    m_buffer.clear();
}

StreamParseResult StreamParser::appendChunk(const QByteArray& data)
{
    StreamParseResult result;
    m_buffer.append(data);

    while (true) {
        const QByteArray event = takeNextEvent();
        if (event.isNull()) {
            break;
        }

        const StreamParseResult partial = parseEvent(event);
        result.content += partial.content;
        if (partial.hasError) {
            result.hasError = true;
            if (!partial.errorMessage.isEmpty()) {
                result.errorMessage = partial.errorMessage;
            }
        }
        result.done = result.done || partial.done;
    }

    return result;
}

StreamParseResult StreamParser::flush()
{
    StreamParseResult result;
    if (m_buffer.trimmed().isEmpty()) {
        m_buffer.clear();
        return result;
    }

    result = parseEvent(m_buffer);
    m_buffer.clear();
    return result;
}

StreamParseResult StreamParser::parseEvent(const QByteArray& event) const
{
    StreamParseResult result;
    bool hasDataLine = false;
    const QList<QByteArray> lines = event.split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(':')) {
            continue;
        }
        if (!line.startsWith("data:")) {
            continue;
        }

        hasDataLine = true;
        QByteArray payload = line.mid(5);
        if (payload.startsWith(' ')) {
            payload.remove(0, 1);
        }
        const StreamParseResult partial = parseEventPayload(payload);
        result.content += partial.content;
        if (partial.hasError) {
            result.hasError = true;
            if (!partial.errorMessage.isEmpty()) {
                result.errorMessage = partial.errorMessage;
            }
        }
        result.done = result.done || partial.done;
    }

    if (!hasDataLine) {
        const QByteArray payload = event.trimmed();
        if (payload.startsWith('{')) {
            return parseEventPayload(payload);
        }
    }

    return result;
}

QByteArray StreamParser::takeNextEvent()
{
    int separator = m_buffer.indexOf("\n\n");
    int separatorLength = 2;
    const int crlfSeparator = m_buffer.indexOf("\r\n\r\n");
    if (crlfSeparator >= 0 && (separator < 0 || crlfSeparator < separator)) {
        separator = crlfSeparator;
        separatorLength = 4;
    }

    if (separator < 0) {
        return QByteArray();
    }

    QByteArray event = m_buffer.left(separator);
    m_buffer.remove(0, separator + separatorLength);
    return event;
}

StreamParseResult StreamParser::parseEventPayload(const QByteArray& payload) const
{
    StreamParseResult result;
    const QByteArray trimmedPayload = payload.trimmed();
    if (trimmedPayload.isEmpty()) {
        return result;
    }
    if (trimmedPayload == "[DONE]") {
        result.done = true;
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmedPayload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.hasError = true;
        result.errorMessage = parseError.errorString();
        return result;
    }

    const QJsonObject obj = doc.object();
    if (obj.contains("error")) {
        const QJsonValue errorValue = obj.value("error");
        result.hasError = true;
        if (errorValue.isObject()) {
            result.errorMessage = errorValue.toObject().value("message").toString("Request failed");
        } else {
            result.errorMessage = errorValue.toString("Request failed");
        }
        return result;
    }

    const QJsonArray choices = obj.value("choices").toArray();
    if (!choices.isEmpty()) {
        const QJsonObject choice = choices.at(0).toObject();
        const QJsonObject delta = choice.value("delta").toObject();
        result.content = delta.value("content").toString();
    }

    return result;
}

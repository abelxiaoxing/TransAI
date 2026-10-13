#include "translation/StreamParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "providers/ApiError.h"

void StreamParser::reset()
{
    m_buffer.clear();
}

void StreamParser::mergeInto(StreamParseResult& target, const StreamParseResult& extra)
{
    target.content += extra.content;
    if (extra.hasError) {
        target.hasError = true;
        if (!extra.errorMessage.isEmpty()) {
            target.errorMessage = extra.errorMessage;
        }
    }
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
        mergeInto(result, parseEvent(event));
    }

    return result;
}

StreamParseResult StreamParser::consume(const QByteArray& data)
{
    StreamParseResult result = appendChunk(data);
    mergeInto(result, flush());
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
        mergeInto(result, parseEventPayload(payload));
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
    // Terminal SSE marker: recognized so it is never parsed as JSON.
    if (trimmedPayload == "[DONE]") {
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
        result.hasError = true;
        result.errorMessage = ApiError::messageFromValue(obj.value("error"), QStringLiteral("Request failed"));
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

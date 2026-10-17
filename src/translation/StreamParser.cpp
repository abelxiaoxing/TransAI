#include "translation/StreamParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "providers/ApiError.h"

namespace {
// Reasoning-block tags emitted inside the content stream by thinking models
// that ignore (or never received) suppression parameters: DeepSeek-R1, QwQ,
// Qwen3, GLM and most OpenAI-compatible relays use <think>; a few vendors use
// one of the variants.
const QStringList kThinkOpenTags = {
    QStringLiteral("<think>"),
    QStringLiteral("<thinking>"),
    QStringLiteral("<reasoning>"),
    QStringLiteral("<thought>"),
};

QString closeTagFor(const QString& openTag)
{
    return openTag.left(1) + QLatin1String("/") + openTag.mid(1);
}

// Length of the longest suffix of `text` that is a proper prefix of one of
// `tags`. Such a suffix must be withheld until the next chunk, because it
// could turn out to be a tag split across chunk boundaries. 11 covers the
// longest candidate ("</reasoning>" minus one character).
int partialTagSuffixLength(const QString& text, const QStringList& tags)
{
    const int maxLen = qMin(text.size(), 11);
    for (int len = maxLen; len >= 1; --len) {
        const QString tail = text.right(len);
        for (const QString& tag : tags) {
            if (tag.startsWith(tail)) {
                return len;
            }
        }
    }
    return 0;
}
} // namespace

void StreamParser::reset()
{
    m_buffer.clear();
    m_inThinkBlock = false;
    m_openTag.clear();
    m_closeTag.clear();
    m_tagHold.clear();
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
    const QByteArray trimmed = m_buffer.trimmed();
    m_buffer.clear();
    if (!trimmed.isEmpty()) {
        result = parseEvent(trimmed);
    }

    // End of stream: the withheld tail can no longer be a tag. Outside a
    // think block it is ordinary content and must be released; inside one it
    // is reasoning from an unclosed block and stays dropped.
    if (!m_tagHold.isEmpty()) {
        if (!m_inThinkBlock) {
            result.content += m_tagHold;
        }
        m_tagHold.clear();
    }

    return result;
}

QString StreamParser::filterThinkBlocks(QString content)
{
    // A tail withheld by the previous chunk may complete (or disprove) a
    // split tag, so it always rejoins the stream here first.
    content.prepend(m_tagHold);
    m_tagHold.clear();

    QString output;
    while (!content.isEmpty()) {
        if (m_inThinkBlock) {
            // Inside a block everything is dropped until the close tag.
            const int closeIndex = content.indexOf(m_closeTag);
            if (closeIndex < 0) {
                // No close tag in sight: drop the reasoning, but withhold a
                // tail that could be the start of a split close tag. Take the
                // tail BEFORE any modification of `content` (a chop() first
                // would make right() return the wrong characters and the real
                // close tag would never match, silently swallowing the rest
                // of the stream).
                m_tagHold = content.right(partialTagSuffixLength(content, { m_closeTag }));
                break;
            }
            content.remove(0, closeIndex + m_closeTag.size());
            m_inThinkBlock = false;
            m_openTag.clear();
            m_closeTag.clear();
        } else {
            int openIndex = -1;
            QString matchedTag;
            for (const QString& tag : kThinkOpenTags) {
                const int index = content.indexOf(tag);
                if (index >= 0 && (openIndex < 0 || index < openIndex)) {
                    openIndex = index;
                    matchedTag = tag;
                }
            }
            if (openIndex < 0) {
                const int hold = partialTagSuffixLength(content, kThinkOpenTags);
                output += content.left(content.size() - hold);
                m_tagHold = content.right(hold);
                break;
            }
            output += content.left(openIndex);
            m_inThinkBlock = true;
            m_openTag = matchedTag;
            m_closeTag = closeTagFor(matchedTag);
            content.remove(0, openIndex + matchedTag.size());
        }
    }

    return output;
}

StreamParseResult StreamParser::parseEvent(const QByteArray& event)
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

StreamParseResult StreamParser::parseEventPayload(const QByteArray& payload)
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
        // Strip reasoning blocks that models emit in the content stream even
        // when suppression was requested (last-resort output-level filter).
        result.content = filterThinkBlocks(delta.value("content").toString());
    }

    return result;
}

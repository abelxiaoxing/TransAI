#ifndef STREAM_PARSER_H
#define STREAM_PARSER_H

#include <QByteArray>
#include <QString>

struct StreamParseResult {
    QString content;
    QString errorMessage;
    bool hasError = false;
};

class StreamParser
{
public:
    void reset();
    StreamParseResult appendChunk(const QByteArray& data);
    StreamParseResult flush();
    // Consumes the closing chunk of a stream: parses `data` and then flushes
    // the buffered tail, merging both into one result. Equivalent to calling
    // appendChunk(data) followed by flush() and combining the results.
    StreamParseResult consume(const QByteArray& data);

private:
    static void mergeInto(StreamParseResult& target, const StreamParseResult& extra);
    StreamParseResult parseEvent(const QByteArray& event);
    StreamParseResult parseEventPayload(const QByteArray& payload);
    QByteArray takeNextEvent();
    QString filterThinkBlocks(QString content);

    QByteArray m_buffer;

    // Think-block filter state (see filterThinkBlocks). Tracks whether the
    // stream is currently inside a <think>-style block, which tag opened it,
    // and a tail withheld from the previous chunk that might turn out to be
    // the beginning of a tag.
    bool m_inThinkBlock = false;
    QString m_openTag;
    QString m_closeTag;
    QString m_tagHold;
};

#endif // STREAM_PARSER_H

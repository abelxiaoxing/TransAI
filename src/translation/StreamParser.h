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
    StreamParseResult parseEvent(const QByteArray& event) const;
    StreamParseResult parseEventPayload(const QByteArray& payload) const;
    QByteArray takeNextEvent();

    QByteArray m_buffer;
};

#endif // STREAM_PARSER_H

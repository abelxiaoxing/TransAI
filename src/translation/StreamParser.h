#ifndef STREAM_PARSER_H
#define STREAM_PARSER_H

#include <QByteArray>
#include <QString>

struct StreamParseResult {
    QString content;
    QString errorMessage;
    bool hasError = false;
    bool done = false;
};

class StreamParser
{
public:
    void reset();
    StreamParseResult appendChunk(const QByteArray& data);
    StreamParseResult flush();

private:
    QByteArray m_buffer;
    StreamParseResult parseEvent(const QByteArray& event) const;
    StreamParseResult parseEventPayload(const QByteArray& payload) const;
    QByteArray takeNextEvent();
};

#endif // STREAM_PARSER_H

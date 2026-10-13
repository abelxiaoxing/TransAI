#include <QtTest>

#include "translation/StreamParser.h"

namespace {
QByteArray sseData(const QByteArray& json)
{
    return "data: " + json + "\n\n";
}

QByteArray sseDelta(const QString& content)
{
    return sseData(QByteArray("{\"choices\":[{\"delta\":{\"content\":\"" + content.toUtf8() + "\"}}]}"));
}
} // namespace

class TestStreamParser : public QObject
{
    Q_OBJECT

private slots:
    void singleCompleteChunk();
    void chunkSplitAcrossReads();
    void doneMarkerIgnored();
    void crlfSeparators();
    void commentLinesIgnored();
    void errorPayload();
    void rawJsonFallback();
    void flushTrailingBuffer();
    void consumeCombinesTailAndFlush();
    void consumeSurfacesFlushedError();
    void multipleEventsInOneChunk();
    void resetClearsBuffer();
};

void TestStreamParser::singleCompleteChunk()
{
    StreamParser parser;
    const StreamParseResult result = parser.appendChunk(sseDelta("Hello"));
    QVERIFY(!result.hasError);
    QCOMPARE(result.content, QString("Hello"));
}

void TestStreamParser::chunkSplitAcrossReads()
{
    StreamParser parser;
    const QByteArray event = sseDelta("World");

    // Split mid-JSON: nothing parseable yet, no premature error.
    const StreamParseResult first = parser.appendChunk(event.left(event.size() / 2));
    QVERIFY(!first.hasError);
    QCOMPARE(first.content, QString());

    const StreamParseResult second = parser.appendChunk(event.mid(event.size() / 2));
    QVERIFY(!second.hasError);
    QCOMPARE(second.content, QString("World"));
}

void TestStreamParser::doneMarkerIgnored()
{
    StreamParser parser;
    const StreamParseResult result = parser.appendChunk("data: [DONE]\n\n");
    QVERIFY(!result.hasError);
    QVERIFY(result.content.isEmpty());
}

void TestStreamParser::crlfSeparators()
{
    StreamParser parser;
    const StreamParseResult result = parser.appendChunk(sseDelta("Hi").replace("\n\n", "\r\n\r\n"));
    QVERIFY(!result.hasError);
    QCOMPARE(result.content, QString("Hi"));
}

void TestStreamParser::commentLinesIgnored()
{
    StreamParser parser;
    const StreamParseResult result = parser.appendChunk(": keep-alive\n\n" + sseDelta("ok"));
    QVERIFY(!result.hasError);
    QCOMPARE(result.content, QString("ok"));
}

void TestStreamParser::errorPayload()
{
    StreamParser parser;
    const StreamParseResult result =
        parser.appendChunk(sseData("{\"error\":{\"message\":\"Incorrect API key\"}}"));
    QVERIFY(result.hasError);
    QCOMPARE(result.errorMessage, QString("Incorrect API key"));
}

void TestStreamParser::rawJsonFallback()
{
    // Non-SSE bodies (plain JSON error pages) must surface their message.
    // Mirrors the production flow: appendChunk on finished, then flush().
    StreamParser parser;
    const StreamParseResult streamed = parser.appendChunk(QByteArray("{\"error\":{\"message\":\"bad key\"}}"));
    QVERIFY(!streamed.hasError);
    const StreamParseResult flushed = parser.flush();
    QVERIFY(flushed.hasError);
    QCOMPARE(flushed.errorMessage, QString("bad key"));

    // Bodies that do contain the blank-line separator parse immediately.
    StreamParser inlineParser;
    const StreamParseResult result =
        inlineParser.appendChunk(QByteArray("{\"error\":{\"message\":\"nope\"}}\n\n"));
    QVERIFY(result.hasError);
    QCOMPARE(result.errorMessage, QString("nope"));
}

void TestStreamParser::flushTrailingBuffer()
{
    StreamParser parser;
    // Final event without the trailing blank line still lands in the buffer.
    const StreamParseResult streamed = parser.appendChunk(sseDelta("A"));
    QCOMPARE(streamed.content, QString("A"));

    QByteArray tail = sseDelta("B");
    tail.chop(2); // drop the trailing "\n\n"
    const StreamParseResult buffered = parser.appendChunk(tail);
    QVERIFY(buffered.content.isEmpty());

    const StreamParseResult flushed = parser.flush();
    QVERIFY(!flushed.hasError);
    QCOMPARE(flushed.content, QString("B"));
}

void TestStreamParser::consumeCombinesTailAndFlush()
{
    // Mirrors TranslationService's finished handler: the last network chunk
    // plus whatever stayed buffered merge into one result.
    StreamParser parser;
    const QByteArray tail = sseDelta("B");
    const StreamParseResult buffered = parser.appendChunk(tail.left(tail.size() / 2));
    QVERIFY(buffered.content.isEmpty());

    const StreamParseResult result = parser.consume(tail.mid(tail.size() / 2));
    QVERIFY(!result.hasError);
    QCOMPARE(result.content, QString("B"));
}

void TestStreamParser::consumeSurfacesFlushedError()
{
    // A raw-JSON error body without the SSE separator is flushed by consume().
    StreamParser parser;
    const StreamParseResult result = parser.consume(QByteArray("{\"error\":{\"message\":\"bad key\"}}"));
    QVERIFY(result.hasError);
    QCOMPARE(result.errorMessage, QString("bad key"));
}

void TestStreamParser::multipleEventsInOneChunk()
{
    StreamParser parser;
    const StreamParseResult result = parser.appendChunk(sseDelta("A") + sseDelta("B") + "data: [DONE]\n\n");
    QVERIFY(!result.hasError);
    QCOMPARE(result.content, QString("AB"));
}

void TestStreamParser::resetClearsBuffer()
{
    StreamParser parser;
    // Half an event stays buffered...
    const QByteArray event = sseDelta("X");
    parser.appendChunk(event.left(event.size() / 2));
    parser.reset();
    // ...and must not leak into the next request's stream.
    const StreamParseResult result = parser.appendChunk(sseDelta("Y"));
    QVERIFY(!result.hasError);
    QCOMPARE(result.content, QString("Y"));
}

QTEST_MAIN(TestStreamParser)
#include "tst_streamparser.moc"

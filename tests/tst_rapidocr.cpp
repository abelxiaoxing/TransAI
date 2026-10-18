#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QtTest>

#include "ocr/OcrEngine.h"
#include "ocr/OcrModelLocator.h"

// End-to-end coverage of the bundled RapidOCR backend: render a known image,
// recognise it, and compare the text. Deliberately independent of any
// screenshot/portal code so it exercises only "image in, text out".
//
// The model files are not in git (13 MB of weights, see
// scripts/fetch_ocr_models.sh): the test skips itself when they are absent, so
// `ctest` stays green on a bare checkout while CI (which fetches them) gets the
// real pipeline covered.
class TestRapidOcr : public QObject {
    Q_OBJECT

private slots:
    void recognizesRenderedText();
    void reportsMissingModelsAsUnavailable();

private:
    static QImage renderText(const QString& text);
};

namespace {
constexpr int kFontPixelSize = 40;
}

QImage TestRapidOcr::renderText(const QString& text) {
    QImage image(720, 120, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    QFont font(QStringLiteral("Noto Sans CJK SC"));
    font.setPixelSize(kFontPixelSize);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(32, 78, text);
    painter.end();
    return image;
}

void TestRapidOcr::recognizesRenderedText() {
    std::unique_ptr<OcrEngine> engine = createRapidOcrEngine();
    QVERIFY(engine);
    if (!engine->isAvailable()) {
        QSKIP("bundled OCR models not installed (run scripts/fetch_ocr_models.sh)");
    }

    // ASCII only: the recogniser is deterministic on clean rendered Latin text,
    // while CJK output depends on the system CJK font actually being present.
    const QString expected = QStringLiteral("TransAI OCR 2024");
    QSignalSpy recognizedSpy(engine.get(), &OcrEngine::recognized);
    QSignalSpy failedSpy(engine.get(), &OcrEngine::recognitionFailed);

    engine->recognize(renderText(expected));
    QTRY_VERIFY_WITH_TIMEOUT(recognizedSpy.count() + failedSpy.count() >= 1, 30000);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(recognizedSpy.count(), 1);

    const QString text = recognizedSpy.at(0).at(0).toString();
    // Compare loosely on purpose: the recogniser drops spaces between words and
    // PP-OCR's Latin dictionary confuses the glyph pair I/l ("TransAI" comes
    // back as "TransAl"), which is a model property, not a pipeline bug.
    QString compact = text.toLower();
    compact.remove(QLatin1Char(' '));
    QVERIFY2(compact.contains(QStringLiteral("trans")), qPrintable(text));
    QVERIFY2(compact.contains(QStringLiteral("ocr")), qPrintable(text));
    QVERIFY2(compact.contains(QStringLiteral("2024")), qPrintable(text));
}

void TestRapidOcr::reportsMissingModelsAsUnavailable() {
    // The environment override is authoritative, so pointing it at a
    // non-existent directory hides the real installation for the lifetime of
    // this test and exercises the unavailable contract end to end.
    const QByteArray previous = qgetenv("TRANSAI_OCR_MODEL_DIR");
    QVERIFY(qputenv("TRANSAI_OCR_MODEL_DIR", "/nonexistent/ocr/models"));

    std::unique_ptr<OcrEngine> engine = createRapidOcrEngine();
    QVERIFY(engine);
    QVERIFY(!engine->isAvailable());
    QVERIFY(!engine->unavailableReason().isEmpty());

    QSignalSpy failedSpy(engine.get(), &OcrEngine::recognitionFailed);
    engine->recognize(renderText(QStringLiteral("anything")));
    QTRY_COMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.at(0).at(0).toString(), engine->unavailableReason());

    if (previous.isEmpty()) {
        qunsetenv("TRANSAI_OCR_MODEL_DIR");
    } else {
        qputenv("TRANSAI_OCR_MODEL_DIR", previous);
    }
}

QTEST_MAIN(TestRapidOcr)
#include "tst_rapidocr.moc"

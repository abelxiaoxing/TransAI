#include <QFile>
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QtTest>

#include "ocr/OcrEngine.h"
#include "ocr/OcrModelLocator.h"

// End-to-end coverage of the bundled RapidOCR backend: feed a known screenshot
// crop to the engine and compare the recognized text. Independent of any
// screenshot/portal code, so it exercises only "image in, text out".
//
// The sample image is a committed fixture (tests/data/ocr_sample.png) rather
// than text rendered at run time: QPainter would need a font database, which
// the headless CI runners (notably the Windows offscreen platform) do not have,
// and the whole test would then assert against a blank image.
//
// The model files are also not in git (13 MB of weights, see
// scripts/fetch_ocr_models.sh): the test skips itself when they are absent, so
// `ctest` stays green on a bare checkout while CI (which fetches them) gets the
// real pipeline covered.
class TestRapidOcr : public QObject
{
    Q_OBJECT

private slots:
    void recognizesFromImage();
    void recognizeIsCallableFromQml();
    void reportsMissingModelsAsUnavailable();

private:
    static QImage sample();
};

QImage TestRapidOcr::sample()
{
    QImage image(QStringLiteral(TRANSAI_OCR_TEST_DATA_DIR "/ocr_sample.png"));
    return image.convertToFormat(QImage::Format_RGB32);
}

void TestRapidOcr::recognizesFromImage()
{
    const QImage image = sample();
    QVERIFY2(!image.isNull(), "tests/data/ocr_sample.png is missing or unreadable");

    std::unique_ptr<OcrEngine> engine = createRapidOcrEngine();
    QVERIFY(engine);
    if (!engine->isAvailable()) {
        QSKIP(qPrintable(QStringLiteral("unavailable: %1 (run scripts/fetch_ocr_models.sh)")
                             .arg(engine->unavailableReason())));
    }

    QSignalSpy recognizedSpy(engine.get(), &OcrEngine::recognized);
    QSignalSpy failedSpy(engine.get(), &OcrEngine::recognitionFailed);

    engine->recognize(image);
    QTRY_VERIFY_WITH_TIMEOUT(recognizedSpy.count() + failedSpy.count() >= 1, 30000);

    // Report the engine's own message when it failed: a model-loading problem
    // must not look like a recognition-quality failure.
    QVERIFY2(failedSpy.isEmpty(),
             qPrintable(failedSpy.isEmpty() ? QString() : failedSpy.at(0).at(0).toString()));
    QCOMPARE(recognizedSpy.count(), 1);

    const QString text = recognizedSpy.at(0).at(0).toString();
    // Compare loosely on purpose: PP-OCR's Latin dictionary confuses the glyph
    // pair I/l ("TransAI" comes back as "TransAl"), which is a model property,
    // not a pipeline bug, and the comparison must not depend on the exact
    // spacing the space class (see decodeCtc) produces.
    QString compact = text.toLower();
    compact.remove(QLatin1Char(' '));
    QVERIFY2(compact.contains(QStringLiteral("trans")), qPrintable(text));
    QVERIFY2(compact.contains(QStringLiteral("ocr")), qPrintable(text));
    QVERIFY2(compact.contains(QStringLiteral("2024")), qPrintable(text));
}

void TestRapidOcr::recognizeIsCallableFromQml()
{
    std::unique_ptr<OcrEngine> engine = createRapidOcrEngine();
    QVERIFY(engine);
    if (!engine->isAvailable()) {
        QSKIP(qPrintable(QStringLiteral("unavailable: %1 (run scripts/fetch_ocr_models.sh)")
                             .arg(engine->unavailableReason())));
    }

    // The QML leg is the only caller of recognize() in the app, and it fails in
    // a way no C++ test notices: calling a method that is not a slot /
    // Q_INVOKABLE is *not* a hard error in QML, it just logs
    // "TypeError: Property 'recognize' of object OcrEngine(...) is not a
    // function" and returns undefined — the OCR leg then never emits a signal
    // and the session hangs on "Recognizing text...". This drives the real
    // engine through the real QML call path (including the QVariant -> QImage
    // conversion) so the declaration in OcrEngine.h cannot regress.
    QQmlEngine qml;
    qml.rootContext()->setContextProperty(QStringLiteral("ocrEngine"), engine.get());

    QQmlComponent component(&qml);
    component.setData(R"qml(import QtQml

QtObject {
    property string recognizedText: ""
    property string failure: ""
    signal finished()

    function run(image) {
        ocrEngine.recognize(image)
    }

    Component.onCompleted: {
        ocrEngine.recognized.connect(function(text) {
            recognizedText = text
            finished()
        })
        ocrEngine.recognitionFailed.connect(function(message) {
            failure = message
            finished()
        })
    }
}
)qml",
                     QUrl(QStringLiteral("qrc:/tst_rapidocr/harness.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    std::unique_ptr<QObject> harness(component.create());
    QVERIFY2(harness, qPrintable(component.errorString()));

    QSignalSpy finishedSpy(harness.get(), SIGNAL(finished()));
    const bool invoked = QMetaObject::invokeMethod(harness.get(), "run",
                                                   Q_ARG(QVariant, QVariant::fromValue(sample())));
    QVERIFY(invoked);

    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 30000);
    QCOMPARE(harness->property("failure").toString(), QString());

    // Same loose comparison as recognizesFromImage(): the model confuses I/l and
    // the exact spacing depends on the space class.
    QString compact = harness->property("recognizedText").toString().toLower();
    compact.remove(QLatin1Char(' '));
    QVERIFY2(compact.contains(QStringLiteral("trans")), qPrintable(compact));
    QVERIFY2(compact.contains(QStringLiteral("ocr")), qPrintable(compact));
}

void TestRapidOcr::reportsMissingModelsAsUnavailable()
{
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
    engine->recognize(sample());
    QTRY_COMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.at(0).at(0).toString(), engine->unavailableReason());

    if (previous.isEmpty()) {
        qunsetenv("TRANSAI_OCR_MODEL_DIR");
    } else {
        qputenv("TRANSAI_OCR_MODEL_DIR", previous);
    }
}

// No GUI: QImage decoding needs a QCoreApplication only, so this runs on every
// CI runner without a platform plugin or a font database.
QTEST_GUILESS_MAIN(TestRapidOcr)
#include "tst_rapidocr.moc"

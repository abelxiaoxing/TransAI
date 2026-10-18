#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

#include "ocr/OcrEngine.h"

// Pure-helper tests for the tesseract language-list parsing/selection; the
// engine's unavailable-path behavior without a tesseract binary is covered
// implicitly by parseListLangsOutput returning empty (engine reports
// unavailable and recognize() fails with a fixed message).
class TestOcrEngine : public QObject
{
    Q_OBJECT

private slots:
    void parseListLangsSkipsHeaderAndOsd();
    void parseListLangsHandlesEmpty();
    void chooseLanguagesPrefersChinesePlusEnglish();
    void chooseLanguagesFallsBack();
    void availabilityIsProbedOffTheStartupPath();
};

void TestOcrEngine::parseListLangsSkipsHeaderAndOsd()
{
    const QString output = QStringLiteral(
        "List of available languages in \"/usr/share/tessdata/\" (4):\n"
        "chi_sim\n"
        "eng\n"
        "osd\n"
        "jpn\n");
    QCOMPARE(OcrLanguageSelection::parseListLangsOutput(output),
             QStringList({ "chi_sim", "eng", "jpn" }));
}

void TestOcrEngine::parseListLangsHandlesEmpty()
{
    QCOMPARE(OcrLanguageSelection::parseListLangsOutput(QString()), QStringList());
    QCOMPARE(OcrLanguageSelection::parseListLangsOutput(
                 QStringLiteral("List of available languages (1):\nosd")),
             QStringList());
}

void TestOcrEngine::chooseLanguagesPrefersChinesePlusEnglish()
{
    QCOMPARE(OcrLanguageSelection::chooseLanguages({ "eng", "chi_sim", "osd" }),
             QStringLiteral("chi_sim+eng"));
    QCOMPARE(OcrLanguageSelection::chooseLanguages({ "chi_sim" }),
             QStringLiteral("chi_sim"));
}

void TestOcrEngine::chooseLanguagesFallsBack()
{
    QCOMPARE(OcrLanguageSelection::chooseLanguages({ "eng", "jpn" }), QStringLiteral("eng"));
    QCOMPARE(OcrLanguageSelection::chooseLanguages({ "jpn", "deu" }), QStringLiteral("jpn"));
    QCOMPARE(OcrLanguageSelection::chooseLanguages({}), QString());
}

// The constructor must not block on `tesseract --version` / `--list-langs`;
// availability is published asynchronously through availableChanged() and must
// still match whether the binary exists.
void TestOcrEngine::availabilityIsProbedOffTheStartupPath()
{
    QElapsedTimer elapsed;
    elapsed.start();
    std::unique_ptr<OcrEngine> engine = createTesseractOcrEngine();
    QVERIFY(engine);
    // Constructing the engine only *starts* the probe: no multi-second stall.
    QVERIFY2(elapsed.elapsed() < 1000, qPrintable(QString::number(elapsed.elapsed())));

    QSignalSpy availabilitySpy(engine.get(), &OcrEngine::availableChanged);
    QTRY_VERIFY_WITH_TIMEOUT(availabilitySpy.count() == 1, 15000);

    // The probe result must reflect reality (both branches are fine: CI may or
    // may not ship a tesseract binary).
    const bool hasTesseract = !QStandardPaths::findExecutable(QStringLiteral("tesseract")).isEmpty();
    QCOMPARE(engine->isAvailable(), hasTesseract);

    // Exactly one notification: a second one would mean a duplicate probe
    // completed after the fallback.
    QTest::qWait(200);
    QCOMPARE(availabilitySpy.count(), 1);
}

QTEST_GUILESS_MAIN(TestOcrEngine)
#include "tst_ocrengine.moc"

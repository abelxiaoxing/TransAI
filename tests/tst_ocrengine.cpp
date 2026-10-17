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

QTEST_GUILESS_MAIN(TestOcrEngine)
#include "tst_ocrengine.moc"

#include <QTemporaryDir>
#include <QtTest>

#include <QDir>
#include <QFile>

#include <cmath>
#include <vector>

#include "ocr/OcrModelLocator.h"
#include "ocr/OcrPostProcess.h"

// Pure-logic coverage for the bundled OCR backend: score-map → boxes, reading
// order, CTC decoding and model discovery. None of it needs ncnn or the model
// files, so this suite runs everywhere (including CI without the models).
class TestOcrPostProcess : public QObject {
    Q_OBJECT

private slots:
    void detectsSingleRegion();
    void ignoresFaintRegions();
    void dropsSpecks();
    void keepsSeparateRegions();
    void scaleBoxesClipsToImage();
    void ordersBoxesForReading();
    void decodesCtcGreedy();
    void decodesCtcRespectsMinimumScore();
    void parsesDictionary();
    void joinsLinesSkipsEmpty();
    void locatorFindsCompleteModelSet();
    void locatorRejectsIncompleteModelSet();
};

namespace {

// Builds a width*height score map with the given axis-aligned rectangles
// painted at `value`.
std::vector<float> scoreMap(int width, int height) {
    return std::vector<float>(size_t(width) * size_t(height), 0.0f);
}

void fillRect(std::vector<float>& map, int width, int x0, int y0, int x1, int y1, float value) {
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            map[size_t(y) * size_t(width) + size_t(x)] = value;
        }
    }
}

}  // namespace

void TestOcrPostProcess::detectsSingleRegion() {
    const int width = 60;
    const int height = 40;
    std::vector<float> map = scoreMap(width, height);
    fillRect(map, width, 10, 12, 35, 20, 0.9f);  // 26 x 9 region

    const auto boxes = OcrPostProcess::boxesFromProbabilityMap(map.data(), width, height);
    QCOMPARE(boxes.size(), size_t(1));

    const auto& box = boxes.front();
    // The unclip expansion grows every side outwards, so the box must be
    // strictly larger than the painted region on both axes.
    QVERIFY(box.width > 26);
    QVERIFY(box.height > 9);
    QVERIFY(box.score > 0.85f && box.score <= 0.9f);
    QVERIFY(box.x <= 10);
    QVERIFY(box.y <= 12);
}

void TestOcrPostProcess::ignoresFaintRegions() {
    const int width = 40;
    const int height = 20;
    std::vector<float> map = scoreMap(width, height);
    fillRect(map, width, 5, 5, 30, 12, 0.2f);  // below the 0.3 binarisation

    QVERIFY(OcrPostProcess::boxesFromProbabilityMap(map.data(), width, height).empty());
}

void TestOcrPostProcess::dropsSpecks() {
    const int width = 20;
    const int height = 20;
    std::vector<float> map = scoreMap(width, height);
    fillRect(map, width, 5, 5, 6, 6, 0.95f);  // 2x2 blob: below minSideLen

    QVERIFY(OcrPostProcess::boxesFromProbabilityMap(map.data(), width, height).empty());
}

void TestOcrPostProcess::keepsSeparateRegions() {
    const int width = 80;
    const int height = 60;
    std::vector<float> map = scoreMap(width, height);
    fillRect(map, width, 5, 5, 30, 12, 0.9f);
    fillRect(map, width, 5, 30, 30, 37, 0.9f);
    fillRect(map, width, 50, 5, 70, 12, 0.9f);

    const auto boxes = OcrPostProcess::boxesFromProbabilityMap(map.data(), width, height);
    QCOMPARE(boxes.size(), size_t(3));
}

void TestOcrPostProcess::scaleBoxesClipsToImage() {
    std::vector<OcrPostProcess::OcrTextBox> boxes = {
        {-10, -10, 40, 20, 0.9f},  // partly outside on the top-left
        {200, 200, 40, 20, 0.9f},  // entirely outside on the bottom-right
        {10, 10, 1, 1, 0.9f},      // degenerate after clipping
    };
    OcrPostProcess::scaleBoxes(boxes, 2.0f, 2.0f, 100, 100);

    QCOMPARE(boxes.size(), size_t(1));
    QCOMPARE(boxes.front().x, 0);
    QCOMPARE(boxes.front().y, 0);
    QCOMPARE(boxes.front().width, 20);
    QCOMPARE(boxes.front().height, 10);
}

void TestOcrPostProcess::ordersBoxesForReading() {
    // Three boxes on two lines, supplied deliberately out of order.
    std::vector<OcrPostProcess::OcrTextBox> boxes = {
        {200, 10, 80, 20, 0.9f},  // line 1, right
        {100, 60, 80, 20, 0.9f},  // line 2
        {10, 12, 80, 18, 0.9f},   // line 1, left
    };
    OcrPostProcess::orderForReading(boxes);

    QCOMPARE(boxes.size(), size_t(3));
    QCOMPARE(boxes[0].x, 10);
    QCOMPARE(boxes[1].x, 200);
    QCOMPARE(boxes[2].x, 100);
}

void TestOcrPostProcess::decodesCtcGreedy() {
    // 2 timesteps, 4 classes (0 = blank, 1 = 'a', 2 = 'b', 3 = 'c').
    const QStringList keys = {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    const std::vector<float> logits = {
        0.1f, 0.7f,  0.1f,  0.1f,   // a
        0.1f, 0.7f,  0.1f,  0.1f,   // a again -> collapsed
        0.1f, 0.1f,  0.8f,  0.1f,   // b
        0.9f, 0.02f, 0.04f, 0.04f,  // blank
        0.1f, 0.1f,  0.1f,  0.7f,   // c
    };
    const auto result = OcrPostProcess::decodeCtc(logits.data(), 5, 4, keys);
    QCOMPARE(result.text, QStringLiteral("abc"));
    QVERIFY(result.score > 0.7f);
}

void TestOcrPostProcess::decodesCtcRespectsMinimumScore() {
    const QStringList keys = {QStringLiteral("a"), QStringLiteral("b")};
    const std::vector<float> logits = {
        0.3f,  0.4f, 0.3f,   // 'a' with a weak 0.40 confidence
        0.05f, 0.1f, 0.85f,  // 'b' with a strong 0.85 confidence
    };
    QCOMPARE(OcrPostProcess::decodeCtc(logits.data(), 2, 3, keys).text, QStringLiteral("ab"));
    QCOMPARE(OcrPostProcess::decodeCtc(logits.data(), 2, 3, keys, 0.5f).text, QStringLiteral("b"));
}

void TestOcrPostProcess::parsesDictionary() {
    const QStringList keys = OcrPostProcess::parseDictionary(QByteArray("a\r\nb\n\n\xe4\xb8\xad\n"));
    QCOMPARE(keys.size(), 3);
    QCOMPARE(keys.at(0), QStringLiteral("a"));
    QCOMPARE(keys.at(1), QStringLiteral("b"));
    QCOMPARE(keys.at(2), QString::fromUtf8("中"));
}

void TestOcrPostProcess::joinsLinesSkipsEmpty() {
    QCOMPARE(OcrPostProcess::joinLines({QStringLiteral("first"), QStringLiteral("  "), QStringLiteral("second")}),
             QStringLiteral("first\nsecond"));
    QCOMPARE(OcrPostProcess::joinLines({}), QString());
}

void TestOcrPostProcess::locatorFindsCompleteModelSet() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QStringList names = {QStringLiteral("ch_PP-OCRv3_det_infer.param"),
                               QStringLiteral("ch_PP-OCRv3_det_infer.bin"),
                               QStringLiteral("ch_PP-OCRv3_rec_infer.param"),
                               QStringLiteral("ch_PP-OCRv3_rec_infer.bin"), QStringLiteral("ppocr_keys_v1.txt")};
    for (const QString& name : names) {
        QFile file(QDir(dir.path()).filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
    }

    const auto paths = OcrModelLocator::locateFrom({QStringLiteral("/nonexistent/place"), dir.path()});
    QVERIFY(paths.isValid());
    QCOMPARE(QDir(paths.directory()).absolutePath(), QDir(dir.path()).absolutePath());
    QCOMPARE(paths.detBin, QDir(dir.path()).filePath(names.at(1)));
}

void TestOcrPostProcess::locatorRejectsIncompleteModelSet() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile file(QDir(dir.path()).filePath(QStringLiteral("ppocr_keys_v1.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));

    // An incomplete candidate is rejected rather than returned half-filled.
    QVERIFY(!OcrModelLocator::locateFrom({dir.path()}).isValid());
    QVERIFY(!OcrModelLocator::locateFrom({QStringLiteral("/nonexistent/place")}).isValid());

    // missingFiles() is what the diagnostics use: a set with only the
    // dictionary present reports the four weight files.
    OcrModelLocator::OcrModelPaths partial;
    partial.keys = QDir(dir.path()).filePath(QStringLiteral("ppocr_keys_v1.txt"));
    QCOMPARE(partial.missingFiles().size(), 4);
    QVERIFY(!partial.isValid());
}

QTEST_GUILESS_MAIN(TestOcrPostProcess)
#include "tst_ocrpostprocess.moc"

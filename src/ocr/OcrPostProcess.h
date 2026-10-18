#ifndef OCR_POST_PROCESS_H
#define OCR_POST_PROCESS_H

#include <QString>
#include <QStringList>

#include <vector>

// Pure, dependency-free post-processing for the bundled RapidOCR (PP-OCR)
// backend. Nothing in here touches ncnn, Qt GUI or the filesystem, so all of it
// is unit-tested without models (tests/tst_ocrpostprocess.cpp).
//
// Three stages, deliberately axis-aligned instead of the OpenCV reference
// implementation (RapidOcrNcnn) that ships with the models:
//   1. probability map  -> text boxes  (boxesFromProbabilityMap)
//   2. boxes           -> reading order (orderForReading)
//   3. CTC logits      -> text          (decodeCtc)
// Rotated text therefore yields an axis-aligned box: good enough for
// screenshots (UI text is horizontal) and it removes OpenCV, the clipper
// polygon-offset library and the angle-classifier model (~1 MB) from the
// bundle.
namespace OcrPostProcess {

// A detected text region in *probability-map* or *source-image* pixels,
// depending on which stage produced it.
struct OcrTextBox {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    float score = 0.0f;
};

// Thresholds mirror PaddleOCR's defaults as used by RapidOCR's CLI:
//   --boxThresh 0.3 --boxScoreThresh 0.5 --unClipRatio 1.6
struct DetParams {
    float boxThresh = 0.3f;       // binarisation threshold for the score map
    float boxScoreThresh = 0.5f;  // minimum mean probability inside a region
    float unclipRatio = 1.6f;     // outward expansion factor (PaddleOCR unclip)
    int minSideLen = 3;           // drop specks smaller than this on the long side
    int maxBoxes = 1000;          // hard cap, keeps pathological input bounded
};

// 8-connected component labelling over the binarised score map. `prob` is a
// width*height float buffer in [0, 1] (the network already applied sigmoid).
// Boxes are in probability-map coordinates; map them back with scaleBoxes().
std::vector<OcrTextBox> boxesFromProbabilityMap(const float* prob, int width, int height,
                                                const DetParams& params = DetParams());

// Maps boxes from probability-map space to source-image space, clipping them to
// the image and dropping degenerate ones.
void scaleBoxes(std::vector<OcrTextBox>& boxes, float scaleX, float scaleY, int sourceWidth, int sourceHeight);

// Sorts boxes into human reading order: line by line from top to bottom, left
// to right inside a line. Two boxes belong to the same line when their vertical
// extents overlap by at least half of the smaller height.
void orderForReading(std::vector<OcrTextBox>& boxes);

struct CtcResult {
    QString text;
    float score = 0.0f;  // mean confidence of the emitted characters (1.0 when empty)
};

// Greedy CTC decoding for the recogniser output: `logits` is a
// timesteps x classes row-major buffer (already softmaxed). Class 0 is the CTC
// blank; class i >= 1 maps to `keys[i - 1]`. Repeated indices are collapsed and
// characters below `minCharScore` are dropped.
CtcResult decodeCtc(const float* logits, int timesteps, int classes, const QStringList& keys,
                    float minCharScore = 0.0f);

// Parses a ppocr_keys_v1.txt style dictionary: one entry per line, UTF-8,
// tolerating CRLF and blank lines. The result is the `keys` argument of
// decodeCtc (the blank class is implicit).
QStringList parseDictionary(const QByteArray& raw);

// Joins per-line results into the block of text handed to the translator,
// dropping empty lines.
QString joinLines(const QStringList& lines);

}  // namespace OcrPostProcess

#endif  // OCR_POST_PROCESS_H

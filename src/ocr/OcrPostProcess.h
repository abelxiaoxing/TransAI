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
//   2. boxes           -> visual rows   (groupRows + mergeRowMembers + joinRowTexts)
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

// Groups boxes into visual text rows: one inner vector per row, members sorted
// left to right, rows top to bottom. Two boxes belong to the same row when
// their vertical extents overlap by at least half of the smaller height.
// The recogniser runs once per *row*, not once per detected box — a settings
// dialog row ("Language" ... "English") is one line of text even though the
// detector splits it at the column gap, and turning every box into its own line
// is what used to send a wall of spurious newlines to the translator.
std::vector<std::vector<OcrTextBox>> groupRows(const std::vector<OcrTextBox>& boxes);

// Merges the members of one row into as few crops as the layout allows: boxes
// closer than `gapFactor * rowHeight` are one run of text (the detector split a
// single line at a wide word gap), and recognizing the union as one image keeps
// spacing and punctuation consistent. Wider gaps stay separate crops (columns);
// their texts are joined afterwards by joinRowTexts().
std::vector<OcrTextBox> mergeRowMembers(const std::vector<OcrTextBox>& row, float gapFactor = 0.5f);

// Joins the recognized texts of one row: a space between parts, except between
// two CJK parts (Chinese/Japanese text has no word spaces, and the recogniser
// never invents one).
QString joinRowTexts(const QStringList& parts);

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

// Safety net for the CJK <-> Latin boundary the recogniser occasionally runs
// together ("再按PaddleOCR的" -> "再按 PaddleOCR 的"). Inter-word spaces come from
// the model's space class (see decodeCtc), so this only covers that one
// boundary, and only against Latin *letters*: Chinese typography does not space
// digits from characters ("2024年", "第3章" stay untouched), and nothing is
// inserted inside a script.
QString normalizeSpacing(const QString& text);

}  // namespace OcrPostProcess

#endif  // OCR_POST_PROCESS_H

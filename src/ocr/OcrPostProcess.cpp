#include "ocr/OcrPostProcess.h"

#include <algorithm>
#include <cmath>

namespace OcrPostProcess {

namespace {

// CJK ideographs, kana and hangul: the scripts that do not separate words with
// spaces (and that PP-OCR's Latin space handling does not apply to).
bool isCjk(const QChar ch) {
    const ushort c = ch.unicode();
    return (c >= 0x3400 && c <= 0x9fff) || (c >= 0xf900 && c <= 0xfaff) || (c >= 0x3040 && c <= 0x30ff) ||
           (c >= 0xac00 && c <= 0xd7af);
}

bool isLatinLetter(const QChar ch) {
    const ushort c = ch.unicode();
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= 0x00c0 && c <= 0x024f);  // Latin-1 supplement + extended
}

// Explicit stack instead of recursion: a full-screen score map is a big
// component and deep recursion would blow the thread stack.
void floodFill(const std::vector<unsigned char>& binary, const float* prob, std::vector<int>& labels, int width,
               int height, int startX, int startY, int label, OcrTextBox& outBox, double& scoreSum, int& pixelCount) {
    std::vector<int> stack;
    stack.push_back(startY * width + startX);
    labels[size_t(startY) * width + startX] = label;

    int minX = startX;
    int maxX = startX;
    int minY = startY;
    int maxY = startY;

    while (!stack.empty()) {
        const int current = stack.back();
        stack.pop_back();
        const int x = current % width;
        const int y = current / width;

        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
        scoreSum += prob[current];
        ++pixelCount;

        for (int dy = -1; dy <= 1; ++dy) {
            const int ny = y + dy;
            if (ny < 0 || ny >= height) {
                continue;
            }
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = x + dx;
                if (nx < 0 || nx >= width) {
                    continue;
                }
                const size_t index = size_t(ny) * width + nx;
                if (!binary[index] || labels[index] >= 0) {
                    continue;
                }
                labels[index] = label;
                stack.push_back(ny * width + nx);
            }
        }
    }

    outBox.x = minX;
    outBox.y = minY;
    outBox.width = maxX - minX + 1;
    outBox.height = maxY - minY + 1;
}

}  // namespace

std::vector<OcrTextBox> boxesFromProbabilityMap(const float* prob, int width, int height, const DetParams& params) {
    std::vector<OcrTextBox> boxes;
    if (!prob || width <= 0 || height <= 0) {
        return boxes;
    }

    const size_t count = size_t(width) * size_t(height);
    std::vector<unsigned char> binary(count, 0);
    for (size_t i = 0; i < count; ++i) {
        binary[i] = prob[i] > params.boxThresh ? 1 : 0;
    }

    std::vector<int> labels(count, -1);
    int nextLabel = 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t index = size_t(y) * width + x;
            if (!binary[index] || labels[index] >= 0) {
                continue;
            }

            OcrTextBox component;
            double scoreSum = 0.0;
            int pixelCount = 0;
            floodFill(binary, prob, labels, width, height, x, y, nextLabel, component, scoreSum, pixelCount);
            ++nextLabel;

            // Mean probability over exactly the component's pixels; the
            // reference implementation averages over the rotated bounding box,
            // which is unavailable (and unnecessary) here.
            const float score = pixelCount > 0 ? float(scoreSum / pixelCount) : 0.0f;

            const int longSide = std::max(component.width, component.height);
            if (longSide < params.minSideLen || score < params.boxScoreThresh) {
                continue;
            }

            // PaddleOCR's unclip: offset every side outwards by
            // area * ratio / perimeter, which for a wide line grows the height
            // much more than the width (exactly what the recogniser wants).
            const double area = double(component.width) * double(component.height);
            const double perimeter = 2.0 * (double(component.width) + double(component.height));
            const double offset = perimeter > 0.0 ? area * params.unclipRatio / perimeter : 0.0;

            OcrTextBox box;
            box.x = int(std::floor(double(component.x) - offset));
            box.y = int(std::floor(double(component.y) - offset));
            box.width = int(std::ceil(double(component.width) + 2.0 * offset));
            box.height = int(std::ceil(double(component.height) + 2.0 * offset));
            box.score = score;
            boxes.push_back(box);

            if (int(boxes.size()) >= params.maxBoxes) {
                return boxes;
            }
        }
    }

    return boxes;
}

void scaleBoxes(std::vector<OcrTextBox>& boxes, float scaleX, float scaleY, int sourceWidth, int sourceHeight) {
    if (scaleX <= 0.0f || scaleY <= 0.0f) {
        boxes.clear();
        return;
    }

    std::vector<OcrTextBox> kept;
    kept.reserve(boxes.size());
    for (OcrTextBox box : boxes) {
        box.x = int(std::lround(box.x / scaleX));
        box.y = int(std::lround(box.y / scaleY));
        box.width = int(std::lround(box.width / scaleX));
        box.height = int(std::lround(box.height / scaleY));

        box.x = std::clamp(box.x, 0, std::max(sourceWidth - 1, 0));
        box.y = std::clamp(box.y, 0, std::max(sourceHeight - 1, 0));
        box.width = std::clamp(box.width, 0, sourceWidth - box.x);
        box.height = std::clamp(box.height, 0, sourceHeight - box.y);
        if (box.width < 2 || box.height < 2) {
            continue;
        }
        kept.push_back(box);
    }
    boxes.swap(kept);
}

std::vector<std::vector<OcrTextBox>> groupRows(const std::vector<OcrTextBox>& boxes) {
    std::vector<std::vector<OcrTextBox>> rows;
    if (boxes.empty()) {
        return rows;
    }

    // Stable sort by vertical centre keeps the comparator a strict weak
    // ordering; the line grouping below then works on a monotonic sequence
    // (a naive "same line if overlapping" comparator would be intransitive).
    std::vector<OcrTextBox> sorted = boxes;
    std::stable_sort(sorted.begin(), sorted.end(), [](const OcrTextBox& a, const OcrTextBox& b) {
        return a.y + a.height / 2 < b.y + b.height / 2;
    });

    for (const OcrTextBox& box : sorted) {
        bool placed = false;
        if (!rows.empty()) {
            auto& row = rows.back();
            int top = row.front().y;
            int bottom = row.front().y + row.front().height;
            for (const OcrTextBox& member : row) {
                top = std::min(top, member.y);
                bottom = std::max(bottom, member.y + member.height);
            }
            const int overlap = std::min(bottom, box.y + box.height) - std::max(top, box.y);
            const int smallerHeight = std::min(bottom - top, box.height);
            if (overlap > 0 && overlap * 2 >= smallerHeight) {
                row.push_back(box);
                placed = true;
            }
        }
        if (!placed) {
            rows.push_back({box});
        }
    }

    for (auto& row : rows) {
        std::stable_sort(row.begin(), row.end(),
                         [](const OcrTextBox& a, const OcrTextBox& b) { return a.x < b.x; });
    }
    return rows;
}

std::vector<OcrTextBox> mergeRowMembers(const std::vector<OcrTextBox>& row, float gapFactor) {
    if (row.size() < 2) {
        return row;
    }

    // Use the row's median height as the scale for "is this gap a word gap?":
    // one stray tall/short box must not change the decision for the whole line.
    std::vector<int> heights;
    heights.reserve(row.size());
    for (const OcrTextBox& box : row) {
        heights.push_back(std::max(box.height, 1));
    }
    std::sort(heights.begin(), heights.end());
    const double medianHeight = double(heights[heights.size() / 2]);
    const double maxGap = std::max(1.0, medianHeight * double(gapFactor));

    std::vector<OcrTextBox> merged;
    for (const OcrTextBox& box : row) {
        if (!merged.empty()) {
            OcrTextBox& last = merged.back();
            const int gap = box.x - (last.x + last.width);
            if (gap <= maxGap) {
                // Union of the two boxes: the row keeps its own y/height range,
                // which is what the recogniser wants as a single line crop.
                const int right = std::max(last.x + last.width, box.x + box.width);
                const int bottom = std::max(last.y + last.height, box.y + box.height);
                last.x = std::min(last.x, box.x);
                last.y = std::min(last.y, box.y);
                last.width = right - last.x;
                last.height = bottom - last.y;
                last.score = std::max(last.score, box.score);
                continue;
            }
        }
        merged.push_back(box);
    }
    return merged;
}

QString joinRowTexts(const QStringList& parts) {
    QString joined;
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (joined.isEmpty()) {
            joined = trimmed;
            continue;
        }
        // No space when both sides are CJK ("第一段" + "第二段" reads wrong with
        // one); everywhere else the parts were separate boxes on one visual row,
        // so a space is the closest thing to the gap the user sees.
        const bool noSpace = isCjk(joined.back()) && isCjk(trimmed.front());
        if (!noSpace) {
            joined += QLatin1Char(' ');
        }
        joined += trimmed;
    }
    return joined;
}

QString normalizeSpacing(const QString& text) {
    QString out;
    out.reserve(text.size() + 8);
    for (int i = 0; i < text.size(); ++i) {
        const QChar current = text.at(i);
        if (!out.isEmpty()) {
            const QChar previous = out.back();
            const bool boundary = (isCjk(previous) && isLatinLetter(current)) ||
                                  (isLatinLetter(previous) && isCjk(current));
            if (boundary) {
                out += QLatin1Char(' ');
            }
        }
        out += current;
    }
    return out;
}

CtcResult decodeCtc(const float* logits, int timesteps, int classes, const QStringList& keys, float minCharScore) {
    CtcResult result;
    result.score = 1.0f;
    if (!logits || timesteps <= 0 || classes <= 0 || keys.isEmpty()) {
        return result;
    }

    // PP-OCR's classifier layout (use_space_char=True, which the bundled
    // ppocr_keys_v1.txt model uses): class 0 is the CTC blank, classes 1..keys
    // are the dictionary and the **last** class is the space. Its dictionary has
    // no space entry, so dropping that class is what turned every English line
    // into "ThescreenshotOCRpipelinedetects..."; the model does predict spaces,
    // it just sits outside `keys`.
    const bool hasSpaceClass = classes == int(keys.size()) + 2;
    const int spaceIndex = int(keys.size()) + 1;

    QString text;
    double scoreSum = 0.0;
    int emitted = 0;
    int lastIndex = 0;

    for (int t = 0; t < timesteps; ++t) {
        const float* row = logits + size_t(t) * size_t(classes);
        int bestIndex = 0;
        float bestValue = row[0];
        for (int c = 1; c < classes; ++c) {
            if (row[c] > bestValue) {
                bestValue = row[c];
                bestIndex = c;
            }
        }

        const bool repeated = t > 0 && bestIndex == lastIndex;
        lastIndex = bestIndex;
        if (bestIndex == 0 || repeated) {
            continue;
        }
        const bool isSpace = hasSpaceClass && bestIndex == spaceIndex;
        if (!isSpace && bestIndex > int(keys.size())) {
            continue;  // unmapped class of an unexpected model
        }
        if (bestValue < minCharScore) {
            continue;
        }
        text += isSpace ? QStringLiteral(" ") : keys[bestIndex - 1];
        scoreSum += bestValue;
        ++emitted;
    }

    result.text = text;
    result.score = emitted > 0 ? float(scoreSum / emitted) : 1.0f;
    return result;
}

QStringList parseDictionary(const QByteArray& raw) {
    QStringList keys;
    const QList<QByteArray> lines = raw.split('\n');
    keys.reserve(lines.size());
    for (QByteArray line : lines) {
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (line.isEmpty()) {
            continue;
        }
        keys << QString::fromUtf8(line);
    }
    return keys;
}

QString joinLines(const QStringList& lines) {
    QStringList kept;
    kept.reserve(lines.size());
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            kept << line;
        }
    }
    return kept.join(QLatin1Char('\n'));
}

}  // namespace OcrPostProcess

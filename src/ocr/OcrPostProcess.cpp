#include "ocr/OcrPostProcess.h"

#include <algorithm>
#include <cmath>

namespace OcrPostProcess {

namespace {

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

void orderForReading(std::vector<OcrTextBox>& boxes) {
    if (boxes.size() < 2) {
        return;
    }

    // Stable sort by vertical centre keeps the comparator a strict weak
    // ordering; the line grouping below then works on a monotonic sequence
    // (a naive "same line if overlapping" comparator would be intransitive).
    std::stable_sort(boxes.begin(), boxes.end(),
                     [](const OcrTextBox& a, const OcrTextBox& b) { return a.y + a.height / 2 < b.y + b.height / 2; });

    std::vector<std::vector<OcrTextBox>> lines;
    for (const OcrTextBox& box : boxes) {
        bool placed = false;
        if (!lines.empty()) {
            auto& line = lines.back();
            int top = line.front().y;
            int bottom = line.front().y + line.front().height;
            for (const OcrTextBox& member : line) {
                top = std::min(top, member.y);
                bottom = std::max(bottom, member.y + member.height);
            }
            const int overlap = std::min(bottom, box.y + box.height) - std::max(top, box.y);
            const int smallerHeight = std::min(bottom - top, box.height);
            if (overlap > 0 && overlap * 2 >= smallerHeight) {
                line.push_back(box);
                placed = true;
            }
        }
        if (!placed) {
            lines.push_back({box});
        }
    }

    size_t total = 0;
    for (const auto& line : lines) {
        total += line.size();
    }
    boxes.clear();
    boxes.reserve(total);
    for (auto& line : lines) {
        std::stable_sort(line.begin(), line.end(), [](const OcrTextBox& a, const OcrTextBox& b) { return a.x < b.x; });
        for (const OcrTextBox& box : line) {
            boxes.push_back(box);
        }
    }
}

CtcResult decodeCtc(const float* logits, int timesteps, int classes, const QStringList& keys, float minCharScore) {
    CtcResult result;
    result.score = 1.0f;
    if (!logits || timesteps <= 0 || classes <= 0 || keys.isEmpty()) {
        return result;
    }

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
        if (bestIndex == 0 || repeated || bestIndex > keys.size()) {
            continue;
        }
        if (bestValue < minCharScore) {
            continue;
        }
        text += keys[bestIndex - 1];
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

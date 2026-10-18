#include "ocr/OcrModelLocator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#ifndef TRANSAI_OCR_SOURCE_MODEL_DIR
#define TRANSAI_OCR_SOURCE_MODEL_DIR ""
#endif

namespace OcrModelLocator {

namespace {

const char* const kDetParam = "ch_PP-OCRv3_det_infer.param";
const char* const kDetBin = "ch_PP-OCRv3_det_infer.bin";
const char* const kRecParam = "ch_PP-OCRv3_rec_infer.param";
const char* const kRecBin = "ch_PP-OCRv3_rec_infer.bin";
const char* const kKeys = "ppocr_keys_v1.txt";

QString join(const QString& dir, const char* name) {
    return QDir(dir).filePath(QLatin1String(name));
}

}  // namespace

bool OcrModelPaths::isValid() const {
    return missingFiles().isEmpty();
}

QStringList OcrModelPaths::missingFiles() const {
    QStringList missing;
    const QList<QPair<QString, QString>> files = {
        {QStringLiteral("det.param"), detParam}, {QStringLiteral("det.bin"), detBin},
        {QStringLiteral("rec.param"), recParam}, {QStringLiteral("rec.bin"), recBin},
        {QStringLiteral("keys"), keys},
    };
    for (const auto& entry : files) {
        if (entry.second.isEmpty() || !QFileInfo(entry.second).isFile()) {
            missing << entry.first;
        }
    }
    return missing;
}

QString OcrModelPaths::directory() const {
    if (detParam.isEmpty()) {
        return QString();
    }
    return QFileInfo(detParam).absolutePath();
}

QStringList candidateDirectories() {
    QStringList dirs;

    // A set override is authoritative: packagers and the tests use it to pin
    // (or, with a bogus path, to suppress) the bundled models instead of
    // falling through to whatever this build happens to find.
    const QString override = qEnvironmentVariable("TRANSAI_OCR_MODEL_DIR");
    if (!override.isEmpty()) {
        return {override};
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        dirs << QDir(appDir).filePath(QStringLiteral("ocr/models"));
        dirs << QDir(appDir).filePath(QStringLiteral("../share/TransAI/ocr/models"));
        dirs << QDir(appDir).filePath(QStringLiteral("../Resources/ocr/models"));
    }

    const QString sourceDir = QString::fromUtf8(TRANSAI_OCR_SOURCE_MODEL_DIR);
    if (!sourceDir.isEmpty() && !dirs.contains(sourceDir)) {
        dirs << sourceDir;
    }

    // Drop duplicates while keeping the order.
    QStringList unique;
    unique.reserve(dirs.size());
    for (const QString& dir : dirs) {
        const QString cleaned = QDir::cleanPath(dir);
        if (!cleaned.isEmpty() && !unique.contains(cleaned)) {
            unique << cleaned;
        }
    }
    return unique;
}

OcrModelPaths locateFrom(const QStringList& directories) {
    for (const QString& dir : directories) {
        if (dir.isEmpty() || !QFileInfo(dir).isDir()) {
            continue;
        }
        OcrModelPaths paths;
        paths.detParam = join(dir, kDetParam);
        paths.detBin = join(dir, kDetBin);
        paths.recParam = join(dir, kRecParam);
        paths.recBin = join(dir, kRecBin);
        paths.keys = join(dir, kKeys);
        if (paths.isValid()) {
            return paths;
        }
    }
    return {};
}

OcrModelPaths locate() {
    return locateFrom(candidateDirectories());
}

}  // namespace OcrModelLocator

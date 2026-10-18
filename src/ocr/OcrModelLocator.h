#ifndef OCR_MODEL_LOCATOR_H
#define OCR_MODEL_LOCATOR_H

#include <QString>
#include <QStringList>

// Locates the bundled RapidOCR/PP-OCR model files that ship inside the
// application directory (never in the Qt resource system: the .bin weights are
// ~13 MB and ncnn maps them straight off disk).
//
// Search order (first complete set wins):
//   1. $TRANSAI_OCR_MODEL_DIR                      developer / packager override
//                                                  (authoritative: when set,
//                                                  nothing else is searched)
//   2. <application dir>/ocr/models                Windows zip, AppImage, AppDir
//   3. <application dir>/../share/TransAI/ocr/models   Linux install layout
//   4. TRANSAI_OCR_SOURCE_MODEL_DIR (build tree)   in-tree builds and tests
namespace OcrModelLocator {

// File names of the four files that make up a model set. They match the
// upstream RapidOCR names so models can be swapped in without renaming.
struct OcrModelPaths {
    QString detParam;
    QString detBin;
    QString recParam;
    QString recBin;
    QString keys;

    bool isValid() const;
    QStringList missingFiles() const;
    // Directory the set was loaded from (empty when invalid).
    QString directory() const;
};

// Directories that will be considered, most specific first. Exposed for
// diagnostics ("where did you look?") and unit tests.
QStringList candidateDirectories();

// Resolves a complete model set from `candidateDirectories()`, or returns an
// invalid OcrModelPaths when none is complete.
OcrModelPaths locate();

// Same resolution against an explicit directory list (unit-testable).
OcrModelPaths locateFrom(const QStringList& directories);

}  // namespace OcrModelLocator

#endif  // OCR_MODEL_LOCATOR_H

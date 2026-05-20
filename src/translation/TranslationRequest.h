#ifndef TRANSLATION_REQUEST_H
#define TRANSLATION_REQUEST_H

#include <QString>
#include <QVariantMap>

enum class TranslationSource {
    ManualInput,
    SelectedText,
    ScreenshotOcr,
    ImageMultimodal
};

enum class TranslationMode {
    Translate,
    ExplainWord,
    CorrectGrammar
};

struct TranslationRequest {
    QString text;
    QString targetLanguage;
    TranslationMode mode = TranslationMode::Translate;
    TranslationSource source = TranslationSource::ManualInput;

    QString provider;
    QString apiServer;
    QString apiKey;
    QString model;
};

TranslationMode translationModeFromString(const QString& value);
TranslationMode translationModeFromInt(int value);
TranslationSource translationSourceFromString(const QString& value);
TranslationRequest translationRequestFromMap(const QVariantMap& map);

#endif // TRANSLATION_REQUEST_H

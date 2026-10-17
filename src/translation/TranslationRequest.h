#ifndef TRANSLATION_REQUEST_H
#define TRANSLATION_REQUEST_H

#include <QString>
#include <QVariantMap>

enum class TranslationMode {
    Translate,
    ExplainWord,
    CorrectGrammar
};

struct TranslationRequest {
    QString text;
    QString targetLanguage;
    TranslationMode mode = TranslationMode::Translate;

    QString provider;
    QString apiServer;
    QString apiKey;
    QString model;
};

TranslationMode translationModeFromInt(int value);
TranslationRequest translationRequestFromMap(const QVariantMap& map);

#endif // TRANSLATION_REQUEST_H

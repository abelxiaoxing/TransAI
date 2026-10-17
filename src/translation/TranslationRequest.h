#ifndef TRANSLATION_REQUEST_H
#define TRANSLATION_REQUEST_H

#include <QString>

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

#endif // TRANSLATION_REQUEST_H

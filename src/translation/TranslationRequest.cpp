#include "translation/TranslationRequest.h"


TranslationMode translationModeFromInt(int value)
{
    switch (value) {
    case 1:
        return TranslationMode::ExplainWord;
    case 2:
        return TranslationMode::CorrectGrammar;
    case 0:
    default:
        return TranslationMode::Translate;
    }
}


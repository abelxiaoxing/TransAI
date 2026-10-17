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

TranslationRequest translationRequestFromMap(const QVariantMap& map)
{
    TranslationRequest request;
    request.text = map.value("text").toString();
    request.targetLanguage = map.value("targetLanguage").toString();
    request.provider = map.value("provider", "openai").toString();
    request.apiServer = map.value("apiServer").toString();
    request.apiKey = map.value("apiKey").toString();
    request.model = map.value("model").toString();

    // The QML UI only ever sends numeric mode codes (see AppView.getMode()).
    request.mode = translationModeFromInt(map.value("mode", 0).toInt());
    return request;
}

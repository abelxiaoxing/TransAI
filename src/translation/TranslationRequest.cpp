#include "translation/TranslationRequest.h"

#include "providers/ThinkingPolicy.h"

namespace {
QString normalizedString(const QString& value)
{
    QString normalized = value.trimmed().toLower();
    normalized.remove('_');
    normalized.remove('-');
    normalized.remove(' ');
    return normalized;
}
}

TranslationMode translationModeFromString(const QString& value)
{
    const QString mode = normalizedString(value);
    if (mode == "1" || mode == "explain" || mode == "explainword" || mode == "word") {
        return TranslationMode::ExplainWord;
    }
    if (mode == "2" || mode == "correct" || mode == "correctgrammar" || mode == "grammar") {
        return TranslationMode::CorrectGrammar;
    }
    return TranslationMode::Translate;
}

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

    const QVariant mode = map.value("mode", 0);
    if (mode.typeId() == QMetaType::Int || mode.typeId() == QMetaType::LongLong || mode.typeId() == QMetaType::UInt || mode.typeId() == QMetaType::ULongLong) {
        request.mode = translationModeFromInt(mode.toInt());
    } else {
        request.mode = translationModeFromString(mode.toString());
    }

    request.thinkingMode =
        ThinkingPolicyBuilder::normalizeThinkingMode(map.value("thinkingMode", "auto").toString());
    return request;
}

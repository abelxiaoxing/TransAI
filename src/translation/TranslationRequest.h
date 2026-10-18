#ifndef TRANSLATION_REQUEST_H
#define TRANSLATION_REQUEST_H

#include <QString>

struct TranslationRequest {
    QString text;
    QString targetLanguage;

    QString provider;
    QString apiServer;
    QString apiKey;
    QString model;
};

#endif // TRANSLATION_REQUEST_H

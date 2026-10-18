#include "app/TranslationHistory.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
const char kHistoryFileName[] = "translation-history.json";
const char kTypeKey[] = "type";
const char kSourceKey[] = "source";
const char kResultKey[] = "result";
const char kTargetLanguageKey[] = "targetLanguage";
const char kTimestampKey[] = "timestamp";
const char kTypeText[] = "text";
const char kTypeScreenshot[] = "screenshot";
}

TranslationHistory::TranslationHistory(QObject* parent)
    : QObject(parent)
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (baseDir.isEmpty()) {
        return;
    }
    QDir dir(baseDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return;
    }
    m_storagePath = dir.filePath(QLatin1String(kHistoryFileName));
    load();
}

void TranslationHistory::addEntry(const QString& type, const QString& source,
                                  const QString& result, const QString& targetLanguage)
{
    if (!available() || result.trimmed().isEmpty()) {
        return;
    }

    QVariantMap entry;
    entry.insert(QLatin1String(kTypeKey),
                 type == QLatin1String(kTypeScreenshot) ? QString::fromLatin1(kTypeScreenshot)
                                                        : QString::fromLatin1(kTypeText));
    entry.insert(QLatin1String(kSourceKey), source);
    entry.insert(QLatin1String(kResultKey), result);
    entry.insert(QLatin1String(kTargetLanguageKey), targetLanguage);
    entry.insert(QLatin1String(kTimestampKey), QDateTime::currentDateTime().toString(Qt::ISODate));

    m_entries.prepend(entry);
    while (m_entries.size() > maxEntries()) {
        m_entries.removeLast();
    }
    save();
    emit entriesChanged();
}

void TranslationHistory::clear()
{
    if (m_entries.isEmpty()) {
        return;
    }
    m_entries.clear();
    save();
    emit entriesChanged();
}

QString TranslationHistory::findCachedResult(const QString& source,
                                             const QString& targetLanguage) const
{
    const QString normalizedSource = source.trimmed();
    if (normalizedSource.isEmpty() || targetLanguage.isEmpty()) {
        return QString();
    }
    for (const QVariant& variant : m_entries) {
        const QVariantMap entry = variant.toMap();
        if (entry.value(QLatin1String(kSourceKey)).toString().trimmed() != normalizedSource) {
            continue;
        }
        if (entry.value(QLatin1String(kTargetLanguageKey)).toString() != targetLanguage) {
            continue;
        }
        const QString result = entry.value(QLatin1String(kResultKey)).toString();
        if (!result.trimmed().isEmpty()) {
            return result;
        }
    }
    return QString();
}

void TranslationHistory::load()
{
    QFile file(m_storagePath);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QByteArray rawData = file.readAll();
    file.close();
    if (rawData.startsWith("\xEF\xBB\xBF")) {
        rawData.remove(0, 3);
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(rawData.trimmed(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        // Same recovery policy as config.json: keep a one-time backup.
        const QString backupPath = m_storagePath + QStringLiteral(".invalid.bak");
        if (!QFileInfo::exists(backupPath)) {
            QFile::copy(m_storagePath, backupPath);
        }
        return;
    }

    const QJsonArray array = doc.array();
    for (const QJsonValue& value : array) {
        const QJsonObject obj = value.toObject();
        const QString result = obj.value(QLatin1String(kResultKey)).toString();
        if (result.trimmed().isEmpty()) {
            continue;
        }
        QVariantMap entry;
        const QString type = obj.value(QLatin1String(kTypeKey)).toString();
        entry.insert(QLatin1String(kTypeKey),
                     type == QLatin1String(kTypeScreenshot) ? QString::fromLatin1(kTypeScreenshot)
                                                            : QString::fromLatin1(kTypeText));
        entry.insert(QLatin1String(kSourceKey), obj.value(QLatin1String(kSourceKey)).toString());
        entry.insert(QLatin1String(kResultKey), result);
        entry.insert(QLatin1String(kTargetLanguageKey), obj.value(QLatin1String(kTargetLanguageKey)).toString());
        entry.insert(QLatin1String(kTimestampKey), obj.value(QLatin1String(kTimestampKey)).toString());
        m_entries.append(entry);
        if (m_entries.size() >= maxEntries()) {
            break;
        }
    }
}

bool TranslationHistory::save() const
{
    if (!available()) {
        return false;
    }

    QJsonArray array;
    for (const QVariant& variant : m_entries) {
        array.append(QJsonObject::fromVariantMap(variant.toMap()));
    }

    QSaveFile file(m_storagePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        return false;
    }
#ifdef Q_OS_UNIX
    // History can contain sensitive text; keep it private like config.json.
    QFile::setPermissions(m_storagePath, QFile::ReadOwner | QFile::WriteOwner);
#endif
    return true;
}

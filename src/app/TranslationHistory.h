#ifndef TRANSLATION_HISTORY_H
#define TRANSLATION_HISTORY_H

#include <QObject>
#include <QVariantList>

// Persists recent translation results (text + screenshot) so the user can
// reopen a previous result after the panel was cleared.
//
// Storage: translation-history.json next to config.json
// (QStandardPaths::AppConfigLocation), newest entry first, capped at 100.
// Written atomically via QSaveFile and chmod 600 on Unix (same policy as
// config.json). A corrupt file is backed up to .invalid.bak and reset,
// mirroring Setting's policy.
//
// Exposed to QML as the "translationHistory" context property (single
// app-owned instance, same pattern as "setting" / "clipboard").
class TranslationHistory : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    // True when the storage location is writable; addEntry() is a silent
    // no-op otherwise (history must never break translation itself).
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit TranslationHistory(QObject* parent = nullptr);

    QVariantList entries() const { return m_entries; }
    bool available() const { return !m_storagePath.isEmpty(); }

    // Records a finished translation. type is "text" or "screenshot"
    // (screenshot entries carry an empty source; the image is never stored).
    // Blank results are ignored.
    Q_INVOKABLE void addEntry(const QString& type, const QString& source,
                              const QString& result, const QString& targetLanguage);
    Q_INVOKABLE void clear();

    // Cache lookup for repeated requests: returns the stored result whose
    // source text (compared after trimming) and target language are identical,
    // or an empty string when nothing matches. The app-owned history is the
    // only cache there is — a repeated sentence (typed twice, or selected and
    // translated again via the hotkey) is answered without a network request
    // and without adding a duplicate entry. Screenshot entries carry no source
    // text, so they never match.
    Q_INVOKABLE QString findCachedResult(const QString& source,
                                        const QString& targetLanguage) const;

    // Storage-limit constant, exposed for tests and future UI hints.
    static constexpr int maxEntries() { return 100; }

signals:
    void entriesChanged();

private:
    void load();
    bool save() const;

    QVariantList m_entries;
    QString m_storagePath;
};

#endif // TRANSLATION_HISTORY_H

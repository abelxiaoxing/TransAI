#ifndef CLIPBOARDHELPER_H
#define CLIPBOARDHELPER_H

#include <QObject>
#include <QString>

// Thin QML-facing wrapper over QClipboard. Exposed as the "clipboard" context
// property (see main.cpp) so views can copy result text without a selection —
// TextEdit.copy() only covers the selected range. Distinct from ClipboardGuard,
// which is the RAII save/restore used by selection translation.
class ClipboardHelper : public QObject
{
    Q_OBJECT
public:
    explicit ClipboardHelper(QObject* parent = nullptr);

    // Writes text to the system clipboard. Empty text is ignored so a
    // mis-tapped copy button never clears the user's clipboard.
    Q_INVOKABLE void setText(const QString& text);
};

#endif // CLIPBOARDHELPER_H

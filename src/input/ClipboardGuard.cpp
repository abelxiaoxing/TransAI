#include "input/ClipboardGuard.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

ClipboardGuard::ClipboardGuard(QObject* parent)
    : QObject(parent)
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard && clipboard->mimeData()) {
        const QMimeData* mimeData = clipboard->mimeData();
        m_hadMimeData = true;
        m_originalText = mimeData->text();
        for (const QString& format : mimeData->formats()) {
            m_originalMimeData.insert(format, mimeData->data(format));
        }
    }
}

QString ClipboardGuard::originalText() const
{
    return m_originalText;
}

QString ClipboardGuard::currentText() const
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    return clipboard ? clipboard->text() : QString();
}

void ClipboardGuard::restore() const
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        return;
    }
    if (!m_hadMimeData) {
        clipboard->clear();
        return;
    }

    QMimeData* restoredMimeData = new QMimeData();
    for (auto it = m_originalMimeData.constBegin(); it != m_originalMimeData.constEnd(); ++it) {
        restoredMimeData->setData(it.key(), it.value());
    }
    if (m_originalMimeData.isEmpty() && !m_originalText.isEmpty()) {
        restoredMimeData->setText(m_originalText);
    }
    // Restoration is best-effort: clipboard updates are delivered
    // asynchronously by the platform and cannot be verified synchronously.
    clipboard->setMimeData(restoredMimeData);
}

#include "input/ClipboardGuard.h"

#include <QClipboard>
#include <QDebug>
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

bool ClipboardGuard::restore() const
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        return false;
    }
    if (!m_hadMimeData) {
        clipboard->clear();
        return clipboard->mimeData() == nullptr || clipboard->mimeData()->formats().isEmpty();
    }

    QMimeData* restoredMimeData = new QMimeData();
    for (auto it = m_originalMimeData.constBegin(); it != m_originalMimeData.constEnd(); ++it) {
        restoredMimeData->setData(it.key(), it.value());
    }
    if (m_originalMimeData.isEmpty() && !m_originalText.isEmpty()) {
        restoredMimeData->setText(m_originalText);
    }
    clipboard->setMimeData(restoredMimeData);
    // Verification is best-effort only: clipboard updates are delivered
    // asynchronously by the platform, so the check may fail spuriously.
    const bool restored = clipboard->text() == m_originalText;
    if (!restored) {
        qWarning() << "Clipboard restore could not be verified synchronously";
    }
    return restored;
}

#include "input/ClipboardGuard.h"

#include <QClipboard>
#include <QGuiApplication>

ClipboardGuard::ClipboardGuard(QObject* parent)
    : QObject(parent)
{
    if (QGuiApplication::clipboard()) {
        m_originalText = QGuiApplication::clipboard()->text();
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
    clipboard->setText(m_originalText);
    return clipboard->text() == m_originalText;
}

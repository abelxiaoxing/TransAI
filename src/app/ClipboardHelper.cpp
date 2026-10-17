#include "app/ClipboardHelper.h"

#include <QClipboard>
#include <QGuiApplication>

ClipboardHelper::ClipboardHelper(QObject* parent)
    : QObject(parent)
{
}

void ClipboardHelper::setText(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(text);
}

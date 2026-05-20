#include "input/SelectionTextProvider.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QPointer>
#include <QTimer>
#include <QUuid>

#include "input/ClipboardGuard.h"

#ifdef Q_OS_MAC
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>
#endif

SelectionTextProvider::SelectionTextProvider(QObject* parent)
    : QObject(parent)
{
}

void SelectionTextProvider::requestSelectedText()
{
    ClipboardGuard* guard = new ClipboardGuard(this);
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        guard->deleteLater();
        emit selectedTextFailed("Clipboard is unavailable");
        return;
    }

    const QString marker = QString("__TRANS_AI_SELECTION_MARKER_%1__")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    clipboard->setText(marker);

    if (!simulateCopy()) {
        guard->restore();
        guard->deleteLater();
        emit selectedTextFailed("Failed to simulate copy");
        return;
    }

    QPointer<ClipboardGuard> guardedClipboard(guard);
    QTimer::singleShot(300, this, [this, guardedClipboard, marker]() {
        if (!guardedClipboard) {
            emit selectedTextFailed("Clipboard guard was destroyed");
            return;
        }

        const QString copiedText = guardedClipboard->currentText();
        const bool clipboardChanged = copiedText != marker;
        guardedClipboard->restore();
        guardedClipboard->deleteLater();

        if (!clipboardChanged) {
            emit selectedTextFailed("Clipboard did not change after copy");
            return;
        }
        if (copiedText.isEmpty()) {
            emit selectedTextFailed("No selected text found");
            return;
        }
        emit selectedTextReady(copiedText);
    });
}

bool SelectionTextProvider::simulateCopy() const
{
#ifdef Q_OS_MAC
    CGEventRef push = CGEventCreateKeyboardEvent(NULL, 0x08, true);
    CGEventSetFlags(push, kCGEventFlagMaskCommand);
    CGEventPost(kCGHIDEventTap, push);
    CFRelease(push);

    push = CGEventCreateKeyboardEvent(NULL, 0x08, false);
    CGEventSetFlags(push, kCGEventFlagMaskCommand);
    CGEventPost(kCGHIDEventTap, push);
    CFRelease(push);
    return true;
#endif

#ifdef Q_OS_WIN
    INPUT inputs[4] = { 0 };
    inputs[0].type = inputs[1].type = inputs[2].type = inputs[3].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = inputs[2].ki.wVk = VK_CONTROL;
    inputs[1].ki.wVk = inputs[3].ki.wVk = 'C';
    inputs[2].ki.dwFlags = inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT)) == ARRAYSIZE(inputs);
#endif

#ifdef Q_OS_LINUX
    if (QGuiApplication::platformName().contains("wayland", Qt::CaseInsensitive)) {
        return false;
    }

    Display* display = XOpenDisplay(NULL);
    if (display == NULL) {
        return false;
    }

    const KeyCode ctrlKey = XKeysymToKeycode(display, XK_Control_L);
    const KeyCode cKey = XKeysymToKeycode(display, XK_c);
    if (ctrlKey == 0 || cKey == 0) {
        XCloseDisplay(display);
        return false;
    }

    const bool ok = XTestFakeKeyEvent(display, ctrlKey, True, CurrentTime)
        && XTestFakeKeyEvent(display, cKey, True, CurrentTime)
        && XTestFakeKeyEvent(display, cKey, False, CurrentTime)
        && XTestFakeKeyEvent(display, ctrlKey, False, CurrentTime);
    XFlush(display);
    usleep(10000);

    XCloseDisplay(display);
    return ok;
#endif

    return false;
}

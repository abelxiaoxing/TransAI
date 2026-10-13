#include "input/SelectionTextProvider.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include "input/ClipboardGuard.h"

#ifdef Q_OS_MACOS
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
    if (QGuiApplication::platformName().contains("wayland", Qt::CaseInsensitive)) {
        requestWaylandSelectedText();
        return;
    }

    requestSelectedTextBySimulatedCopy();
}

void SelectionTextProvider::requestWaylandSelectedText()
{
    if (m_waylandSelectionProcess) {
        disconnect(m_waylandSelectionProcess, nullptr, this, nullptr);
        m_waylandSelectionProcess->kill();
        m_waylandSelectionProcess->deleteLater();
        m_waylandSelectionProcess = nullptr;
    }

    const QString wlPaste = QStandardPaths::findExecutable("wl-paste");
    if (wlPaste.isEmpty()) {
        if (!emitQtPrimarySelection()) {
            failWaylandRequest("wl-paste is not installed");
        }
        return;
    }

    QProcess* process = new QProcess(this);
    m_waylandSelectionProcess = process;
    process->setProgram(wlPaste);
    process->setArguments({"--primary", "--no-newline"});

    connect(process, &QProcess::finished, this,
        [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
            if (m_waylandSelectionProcess != process) {
                return;
            }
            m_waylandSelectionProcess = nullptr;

            const QString selectedText = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
            const QString errorDetail = QString::fromUtf8(process->readAllStandardError()).trimmed();
            process->deleteLater();

            if (exitStatus == QProcess::NormalExit && exitCode == 0 && !selectedText.isEmpty()) {
                emit selectedTextReady(selectedText);
                return;
            }
            failWaylandRequest(errorDetail);
        });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_waylandSelectionProcess != process) {
            return;
        }
        m_waylandSelectionProcess = nullptr;
        const QString detail = process->errorString();
        process->deleteLater();
        if (!emitQtPrimarySelection()) {
            failWaylandRequest(detail);
        }
    });

    process->start();
    QTimer::singleShot(1200, process, [this, process]() {
        if (m_waylandSelectionProcess != process) {
            return;
        }
        m_waylandSelectionProcess = nullptr;
        disconnect(process, nullptr, this, nullptr);
        process->kill();
        process->deleteLater();
        failWaylandRequest("primary selection request timed out");
    });
}

void SelectionTextProvider::requestSelectedTextBySimulatedCopy()
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

    // guard is parented to this, and this is also the singleShot context
    // object, so guard always outlives the callback; no lifetime guard needed.
    QTimer::singleShot(300, this, [this, guard, marker]() {
        const QString copiedText = guard->currentText();
        const bool clipboardChanged = copiedText != marker;
        guard->restore();
        guard->deleteLater();

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

bool SelectionTextProvider::emitQtPrimarySelection()
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard || !clipboard->supportsSelection()) {
        return false;
    }

    const QString selectedText = clipboard->text(QClipboard::Selection).trimmed();
    if (selectedText.isEmpty()) {
        return false;
    }

    emit selectedTextReady(selectedText);
    return true;
}

void SelectionTextProvider::failWaylandRequest(const QString& detail)
{
    QString reason = "No selected text is available. Select text in another application, then trigger translation again.";
    if (!detail.isEmpty()) {
        reason += QString(" (%1)").arg(detail);
    }
    emit selectedTextFailed(reason);
}

bool SelectionTextProvider::simulateCopy() const
{
#ifdef Q_OS_MACOS
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

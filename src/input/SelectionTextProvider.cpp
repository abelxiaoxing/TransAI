#include "input/SelectionTextProvider.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include "input/ClipboardGuard.h"

namespace {
// Ceiling for the simulated copy: the clipboard is usually updated within a
// few tens of milliseconds (QClipboard::dataChanged), this only bounds the wait
// for applications that copy asynchronously.
constexpr int kSimulatedCopyTimeoutMs = 300;
} // namespace

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
    abortPendingCopy();

    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        emit selectedTextFailed("Clipboard is unavailable");
        return;
    }

    // Snapshot the clipboard BEFORE planting the marker so restore() brings the
    // user's original content back.
    m_clipboardGuard = new ClipboardGuard(this);
    m_copyMarker = QString("__TRANS_AI_SELECTION_MARKER_%1__")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    // Event-driven completion: react to the clipboard actually changing instead
    // of always paying a fixed delay (a slow app used to be missed entirely).
    m_clipboardConnection = connect(clipboard, &QClipboard::dataChanged, this,
        [this, clipboard]() {
            if (clipboard->text() != m_copyMarker) {
                completeSimulatedCopy();
            }
        });
    m_clipboardWaitTimer = new QTimer(this);
    m_clipboardWaitTimer->setSingleShot(true);
    connect(m_clipboardWaitTimer, &QTimer::timeout, this,
            &SelectionTextProvider::completeSimulatedCopy);

    clipboard->setText(m_copyMarker);
    if (!simulateCopy()) {
        // The guard restores the snapshot taken above; nothing was copied.
        abortPendingCopy();
        emit selectedTextFailed("Failed to simulate copy");
        return;
    }
    m_clipboardWaitTimer->start(kSimulatedCopyTimeoutMs);
}

void SelectionTextProvider::completeSimulatedCopy()
{
    if (!m_clipboardGuard) {
        return; // already completed (dataChanged and the timer may race)
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    const QString copiedText = clipboard ? clipboard->text() : QString();
    const bool clipboardChanged = clipboard && copiedText != m_copyMarker;
    abortPendingCopy();

    if (!clipboardChanged) {
        emit selectedTextFailed("Clipboard did not change after copy");
        return;
    }
    if (copiedText.isEmpty()) {
        emit selectedTextFailed("No selected text found");
        return;
    }
    emit selectedTextReady(copiedText);
}

void SelectionTextProvider::abortPendingCopy()
{
    if (m_clipboardConnection) {
        disconnect(m_clipboardConnection);
        m_clipboardConnection = QMetaObject::Connection();
    }
    if (m_clipboardWaitTimer) {
        m_clipboardWaitTimer->stop();
        m_clipboardWaitTimer->deleteLater();
        m_clipboardWaitTimer = nullptr;
    }
    if (m_clipboardGuard) {
        // Best-effort: put the user's original clipboard content back.
        m_clipboardGuard->restore();
        m_clipboardGuard->deleteLater();
        m_clipboardGuard = nullptr;
    }
    m_copyMarker.clear();
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

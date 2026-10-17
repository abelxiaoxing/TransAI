import QtQuick
import QtQuick.Controls
import ".."

// "Global Hotkey" card: selection-translation hotkey recorder plus the
// Wayland guidance (command hint with copy button, click-outside capability
// notice). Extracted from SettingView.qml; the recorder now reuses the
// shared ShortcutRecorder component (behavior equivalent: accept/clear fire
// edited(), cancel restores the previous registration).
SectionCard {
    id: section
    title: I18n.t("Global Hotkey")

    readonly property color mutedForeground: ThemeColors.mutedForeground
    readonly property color infoText: ThemeColors.infoText
    readonly property color errorColor: ThemeColors.error
    readonly property int fontSizeLabel: 13

    property alias shortcut: recorder.shortcut
    // Last hotkey-registration failure, shown inline. main.qml also writes
    // here (through SettingView's alias) when registration fails outside a
    // recording session.
    property string hotkeyError: ""
    property bool hotkeysSupported: true
    property QtObject shortcutRegistrar: null

    // Shortcut text changed (user accepted/cleared); page debounces the save.
    signal edited()

    function releaseFocus() {
        recorder.focus = false
    }

    // Background-click hook: re-apply the current shortcut (a cancelled
    // recording session may have left the registration cleared).
    function reapplyShortcut() {
        if (shortcut.length > 0 && shortcutRegistrar !== null) {
            shortcutRegistrar.setShortcut(shortcut)
        }
    }

    Rectangle {
        width: parent.width
        visible: !section.hotkeysSupported
        height: visible ? waylandTip.implicitHeight + 18 : 0
        radius: 10
        color: Qt.alpha(section.infoText, 0.08)
        border.width: 1
        border.color: Qt.alpha(section.infoText, 0.25)

        Text {
            id: waylandTip
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            text: I18n.t("Global hotkeys aren't available on Wayland. Instead, bind a shortcut in your desktop settings to run: TransAI --translate-selection")
            color: section.infoText
            font.pixelSize: section.fontSizeLabel
            wrapMode: Text.WordWrap
            lineHeight: 1.25
        }
    }

    // Lets the user copy the exact CLI command into their compositor hotkey
    // config (Wayland has no in-app hotkeys).
    Text {
        visible: !section.hotkeysSupported
        text: selectionCopyFeedback.running ? I18n.t("Copied ✓") : I18n.t("Copy command")
        color: selectionCopyFeedback.running ? section.accent
             : (selectionCopyArea.containsMouse ? section.accentHover : section.mutedForeground)
        font.pixelSize: section.fontSizeLabel
        font.weight: Font.DemiBold
        MouseArea {
            id: selectionCopyArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                clipboard.setText("TransAI --translate-selection")
                selectionCopyFeedback.restart()
            }
        }
        Timer {
            id: selectionCopyFeedback
            interval: 1500
        }
    }

    // Click-outside-to-hide on Wayland needs read access to the evdev device
    // nodes; without it the window silently stays up (Esc / ✕ still work).
    // Surface the degraded state instead.
    Text {
        visible: !section.hotkeysSupported && !globalClickMonitor.available
        width: parent.width
        text: I18n.t("Click-outside-to-hide is unavailable: TransAI can't read the pointer devices (/dev/input). See docs/hyprland-integration.md for the udev rule.")
        color: section.mutedForeground
        font.pixelSize: section.fontSizeLabel
        wrapMode: Text.WordWrap
        lineHeight: 1.25
    }

    ShortcutRecorder {
        id: recorder
        width: parent.width
        registrar: section.shortcutRegistrar
        hotkeysSupported: section.hotkeysSupported
        onEdited: section.edited()
        onErrorTextChanged: section.hotkeyError = errorText
    }

    Text {
        width: parent.width
        visible: section.hotkeysSupported && section.hotkeyError.length > 0
        text: section.hotkeyError
        color: section.errorColor
        font.pixelSize: section.fontSizeLabel
        wrapMode: Text.WordWrap
        lineHeight: 1.25
    }
}

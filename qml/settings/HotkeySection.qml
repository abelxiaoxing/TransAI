import QtQuick
import QtQuick.Controls
import ".."

// "Global Hotkey" card: both hotkey recorders (selection translation and
// screenshot translation) plus the shared Wayland guidance (one hint box
// listing both CLI commands with copy buttons, and the click-outside
// capability notice). The recorders reuse the shared ShortcutRecorder
// component (behavior equivalent: accept/clear fire edited(), cancel
// restores the previous registration).
SectionCard {
    id: section
    title: I18n.t("Global Hotkey")

    readonly property color mutedForeground: ThemeColors.mutedForeground
    readonly property color infoText: ThemeColors.infoText
    readonly property color errorColor: ThemeColors.error
    readonly property int fontSizeLabel: 13

    // Selection-translation hotkey (action id "translateSelection").
    property alias shortcut: selectionRecorder.shortcut
    // Screenshot-translation hotkey (action id "translateScreenshot").
    property alias screenshotShortcut: screenshotRecorder.shortcut
    // Last hotkey-registration failure, shown inline. Aliased to the
    // recorders so main.qml can write startup failures straight into their
    // display.
    property alias hotkeyError: selectionRecorder.errorText
    property alias screenshotHotkeyError: screenshotRecorder.errorText
    property bool hotkeysSupported: true
    property QtObject shortcutRegistrar: null

    // Shortcut text changed (user accepted/cleared); page debounces the save.
    signal edited()

    function releaseFocus() {
        selectionRecorder.focus = false
        screenshotRecorder.focus = false
    }

    // Background-click hook: re-apply the current shortcuts (a cancelled
    // recording session may have left a registration cleared).
    function reapplyShortcut() {
        if (shortcutRegistrar === null) {
            return
        }
        if (shortcut.length > 0) {
            shortcutRegistrar.setShortcut(selectionRecorder.actionId, shortcut)
        }
        if (screenshotShortcut.length > 0) {
            shortcutRegistrar.setShortcut(screenshotRecorder.actionId, screenshotShortcut)
        }
    }

    SettingsRow {
        label: I18n.t("SELECTION")

        ShortcutRecorder {
            id: selectionRecorder
            width: parent.width
            // Action id this recorder binds; GlobalShortcutManager uses it as
            // the slot for both registration and conflict arbitration.
            actionId: "translateSelection"
            registrar: section.shortcutRegistrar
            hotkeysSupported: section.hotkeysSupported
            onEdited: section.edited()
        }
    }

    Text {
        width: parent.width
        // 缩进到控件列：错误从属于录制器，与其左缘对齐。
        leftPadding: section.fieldColumnX
        visible: section.hotkeysSupported && section.hotkeyError.length > 0
        // Fixed English reasons from C++ translate through the I18n table;
        // dynamic ones fall through unchanged.
        text: I18n.t(section.hotkeyError)
        color: section.errorColor
        font.pixelSize: section.fontSizeLabel
        wrapMode: Text.WordWrap
        lineHeight: 1.25
    }

    SettingsRow {
        label: I18n.t("SCREENSHOT")

        ShortcutRecorder {
            id: screenshotRecorder
            width: parent.width
            actionId: "translateScreenshot"
            registrar: section.shortcutRegistrar
            hotkeysSupported: section.hotkeysSupported
            onEdited: section.edited()
        }
    }

    Text {
        width: parent.width
        leftPadding: section.fieldColumnX
        // Same gate as the selection error: on Wayland/headless the Wayland
        // notice below already explains why nothing could be registered.
        visible: section.hotkeysSupported && section.screenshotHotkeyError.length > 0
        // Fixed English reasons from C++ translate through the I18n table.
        text: I18n.t(section.screenshotHotkeyError)
        color: section.errorColor
        font.pixelSize: section.fontSizeLabel
        wrapMode: Text.WordWrap
        lineHeight: 1.25
    }

    // Card-wide Wayland notice: in-app registration is impossible there, so
    // both recorders degrade to display-only and the user binds the CLI
    // commands in the compositor instead. One box lists both commands.
    Rectangle {
        width: parent.width
        visible: !section.hotkeysSupported
        height: visible ? waylandColumn.implicitHeight + 20 : 0
        radius: 10
        color: Qt.alpha(section.infoText, 0.08)
        border.width: 1
        border.color: Qt.alpha(section.infoText, 0.25)

        Column {
            id: waylandColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8

            Text {
                width: parent.width
                text: I18n.t("Global hotkeys aren't available on Wayland. Bind shortcuts in your desktop settings to run these commands:")
                color: section.infoText
                font.pixelSize: section.fontSizeLabel
                wrapMode: Text.WordWrap
                lineHeight: 1.25
            }

            // Lets the user copy the exact CLI command into their compositor
            // hotkey config (Wayland has no in-app hotkeys).
            Row {
                width: parent.width
                spacing: 8

                Text {
                    width: parent.width - selectionCopy.implicitWidth - 8
                    text: "TransAI --translate-selection"
                    color: section.mutedForeground
                    font.pixelSize: section.fontSizeLabel
                    elide: Text.ElideRight
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    id: selectionCopy
                    text: selectionCopyFeedback.running ? I18n.t("Copied ✓") : I18n.t("Copy command")
                    color: selectionCopyFeedback.running ? section.accent
                         : (selectionCopyArea.containsMouse ? section.accentHover : section.mutedForeground)
                    font.pixelSize: section.fontSizeLabel
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Button
                    Accessible.name: I18n.t("Copy command")
                    Accessible.onPressAction: selectionCopyArea.clicked(null)

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
            }

            Row {
                width: parent.width
                spacing: 8

                Text {
                    width: parent.width - screenshotCopy.implicitWidth - 8
                    text: "TransAI --translate-screenshot"
                    color: section.mutedForeground
                    font.pixelSize: section.fontSizeLabel
                    elide: Text.ElideRight
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    id: screenshotCopy
                    text: screenshotCopyFeedback.running ? I18n.t("Copied ✓") : I18n.t("Copy command")
                    color: screenshotCopyFeedback.running ? section.accent
                         : (screenshotCopyArea.containsMouse ? section.accentHover : section.mutedForeground)
                    font.pixelSize: section.fontSizeLabel
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Button
                    Accessible.name: I18n.t("Copy command")
                    Accessible.onPressAction: screenshotCopyArea.clicked(null)

                    MouseArea {
                        id: screenshotCopyArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            clipboard.setText("TransAI --translate-screenshot")
                            screenshotCopyFeedback.restart()
                        }
                    }
                    Timer {
                        id: screenshotCopyFeedback
                        interval: 1500
                    }
                }
            }
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
}

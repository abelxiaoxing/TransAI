import QtQuick
import QtQuick.Controls
import ".."

// Self-contained shortcut recorder (focus/record/cancel/restore cycle).
// `shortcut` is the single data source; `registrar` applies the OS-level
// registration; `edited()` fires after an accepted recording or a clear so
// the parent can persist. Extracted from SettingView.qml (was the inline
// ShortcutRecorder component); the key-event → shortcut-string helpers moved
// in here with it.
Rectangle {
    id: recorder

    readonly property color accent: ThemeColors.accent
    readonly property color foreground: ThemeColors.foreground
    readonly property color mutedForeground: ThemeColors.mutedForeground
    readonly property color subtleForeground: ThemeColors.subtleForeground
    readonly property color fieldColor: ThemeColors.fieldColor
    readonly property color fieldColorFocus: ThemeColors.fieldColorFocus
    readonly property color borderColor: ThemeColors.border
    readonly property color borderActive: ThemeColors.borderActive
    readonly property color error: ThemeColors.error
    readonly property int fontSizeLabel: 13
    readonly property int fontSizeSmall: 14
    readonly property int fontSizeNormal: 15
    readonly property real cornerRadius: 10

    property string shortcut: ""
    // Action id passed to the registrar (GlobalShortcutManager slot).
    property string actionId: ""
    // Registrar contract: setShortcut(actionId, shortcut) -> bool, and a
    // `lastError` string carrying the reason of a rejected attempt.
    property QtObject registrar: null
    property string errorText: ""
    property string beforeRecording: ""
    property bool recordingAccepted: false
    // False on Wayland/headless: the recorder is then display-only.
    property bool hotkeysSupported: true
    signal edited()

    function keyNameFromEvent(event) {
        if ((event.key >= Qt.Key_A && event.key <= Qt.Key_Z)
            || (event.key >= Qt.Key_0 && event.key <= Qt.Key_9)) {
            return String.fromCharCode(event.key)
        }
        // Qt.Key_F1..Qt.Key_F12 are consecutive values.
        if (event.key >= Qt.Key_F1 && event.key <= Qt.Key_F12) {
            return "F" + (event.key - Qt.Key_F1 + 1)
        }
        return ""
    }

    function shortcutFromEvent(event) {
        var parts = []
        if (event.modifiers & Qt.ControlModifier) {
            parts.push("Ctrl")
        }
        if (event.modifiers & Qt.MetaModifier) {
            parts.push("Meta")
        }
        if (event.modifiers & Qt.AltModifier) {
            parts.push("Alt")
        }
        if (event.modifiers & Qt.ShiftModifier) {
            parts.push("Shift")
        }

        var keyName = keyNameFromEvent(event)
        if (keyName.length === 0) {
            return ""
        }

        var isFunctionKey = keyName.charAt(0) === "F"
        if (parts.length === 0 && !isFunctionKey) {
            return ""
        }
        return parts.length > 0 ? parts.join("+") + "+" + keyName : keyName
    }

    height: 44
    color: activeFocus ? fieldColorFocus : fieldColor
    radius: recorder.cornerRadius
    border.width: 1
    border.color: activeFocus ? borderActive : borderColor
    opacity: hotkeysSupported ? 1.0 : 0.5

    Accessible.role: Accessible.Button
    Accessible.name: recorder.shortcut.length > 0 ? recorder.shortcut : I18n.t("Click to record a shortcut")
    Accessible.onPressAction: if (recorder.hotkeysSupported) { recorder.forceActiveFocus() }

    onActiveFocusChanged: {
        if (activeFocus) {
            beforeRecording = shortcut
            recordingAccepted = false
            errorText = ""
            if (beforeRecording.length > 0 && registrar !== null) {
                registrar.setShortcut(actionId, "")
            }
        } else if (!recordingAccepted && beforeRecording.length > 0 && registrar !== null) {
            registrar.setShortcut(actionId, beforeRecording)
        }
    }

    Behavior on color { ColorAnimation { duration: 160 } }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: recorder.activeFocus ? 92 : (recorderClear.visible ? 40 : 12)
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6
        clip: true

        Repeater {
            model: !recorder.activeFocus && recorder.shortcut.length > 0 ? recorder.shortcut.split("+") : []

            delegate: Rectangle {
                height: 30
                width: chipText.implicitWidth + 20
                radius: 8
                color: recorder.fieldColorFocus
                border.width: 1
                border.color: Qt.alpha(recorder.accent, 0.35)

                Text {
                    id: chipText
                    anchors.centerIn: parent
                    text: modelData
                    color: recorder.foreground
                    font.pixelSize: recorder.fontSizeSmall
                    font.weight: Font.DemiBold
                }
            }
        }

        Text {
            visible: recorder.shortcut.length === 0 && !recorder.activeFocus
            text: recorder.hotkeysSupported ? I18n.t("Click to record a shortcut") : I18n.t("Unavailable on this platform")
            color: recorder.subtleForeground
            font.pixelSize: recorder.fontSizeSmall
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            visible: recorder.activeFocus
            text: I18n.t("Press keys…")
            color: recorder.accent
            font.pixelSize: recorder.fontSizeSmall
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        visible: recorder.activeFocus
        text: I18n.t("Esc to cancel")
        color: recorder.subtleForeground
        font.pixelSize: recorder.fontSizeLabel
    }

    Keys.onPressed: (event) => {
        if (!recorder.activeFocus) {
            return
        }

        if (event.key === Qt.Key_Escape) {
            recorder.focus = false
            event.accepted = true
            return
        }

        var shortcut = shortcutFromEvent(event)
        if (shortcut.length > 0) {
            if (recorder.registrar !== null && recorder.registrar.setShortcut(recorder.actionId, shortcut) === false) {
                // Prefer the registration funnel's reason (fixed, translatable
                // strings such as "already assigned to …"); fall back to the
                // generic wording when it has none.
                var reason = recorder.registrar.lastError !== undefined ? recorder.registrar.lastError : ""
                recorder.errorText = reason.length > 0 ? reason
                    : I18n.t("Couldn't register '%1'. It may be reserved by the system or another app.", shortcut)
                if (recorder.beforeRecording.length > 0) {
                    recorder.registrar.setShortcut(recorder.actionId, recorder.beforeRecording)
                }
                recorder.focus = false
                event.accepted = true
                return
            }
            recorder.recordingAccepted = true
            recorder.errorText = ""
            recorder.shortcut = shortcut
            recorder.edited()
            recorder.focus = false
        }
        event.accepted = true
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: recorder.hotkeysSupported ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            if (recorder.hotkeysSupported) {
                recorder.forceActiveFocus()
            }
        }
    }

    Text {
        id: recorderClear
        anchors.right: parent.right
        anchors.rightMargin: 13
        anchors.verticalCenter: parent.verticalCenter
        visible: recorder.hotkeysSupported && !recorder.activeFocus && recorder.shortcut.length > 0
        text: "✕"
        color: recorderClearArea.containsMouse ? recorder.error : recorder.mutedForeground
        font.pixelSize: recorder.fontSizeNormal
        Accessible.role: Accessible.Button
        Accessible.name: I18n.t("Clear")
        Accessible.onPressAction: recorderClearArea.clicked(null)

        MouseArea {
            id: recorderClearArea
            anchors.fill: parent
            anchors.margins: -8
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (recorder.registrar !== null) {
                    recorder.registrar.setShortcut(recorder.actionId, "")
                }
                recorder.errorText = ""
                recorder.shortcut = ""
                recorder.edited()
            }
        }
    }
}

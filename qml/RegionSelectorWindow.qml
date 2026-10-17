import QtQuick
import QtQuick.Window
import TransAI
import "."

// Fullscreen per-screen overlay for in-app region selection (non-Wayland
// screenshot path). Shows the frozen screen grab dimmed; the user drags a
// rectangle and the selection (in preview-image pixels) is reported via
// accepted(). Esc / right-click cancels the whole capture.
//
// Created dynamically by main.qml, one instance per screen; destroyed by the
// creator after accepted()/canceled().
Window {
    id: overlay

    property int screenIndex: -1
    property string imageDataUrl: ""
    property int previewWidth: 0
    property int previewHeight: 0

    signal accepted(int idx, int x, int y, int w, int h)
    signal canceled()

    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visibility: Window.FullScreen
    color: "black"
    visible: true

    Component.onCompleted: overlay.requestActivate()

    Item {
        id: content
        anchors.fill: parent
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                overlay.canceled()
                event.accepted = true
            }
        }

        Image {
            anchors.fill: parent
            source: overlay.imageDataUrl
            fillMode: Image.Stretch
            cache: false
            smooth: true
        }

        // Dim everything except the current selection (4 rects around it).
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: selection.height > 0 ? selection.y : parent.height
            color: "#99000000"
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: selection.height > 0 ? parent.height - selection.y - selection.height : 0
            color: "#99000000"
            visible: selection.height > 0
        }
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.topMargin: selection.y
            width: selection.x
            height: selection.height
            color: "#99000000"
            visible: selection.width > 0 && selection.height > 0
        }
        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: selection.y
            width: parent.width - selection.x - selection.width
            height: selection.height
            color: "#99000000"
            visible: selection.width > 0 && selection.height > 0
        }

        Rectangle {
            id: selection
            visible: width > 0 && height > 0
            color: "transparent"
            border.color: ThemeColors.accent
            border.width: 2
        }

        // Selection size badge (window pixels; informational only).
        Rectangle {
            visible: selection.width > 0 && selection.height > 0
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: selection.x
            anchors.topMargin: selection.y + selection.height + 6 < parent.height
                ? selection.y + selection.height + 6
                : selection.y - height - 6
            radius: 4
            color: Qt.alpha(ThemeColors.panelColorAlt, 0.95)
            border.width: 1
            border.color: ThemeColors.borderActive
            width: sizeText.implicitWidth + 14
            height: sizeText.implicitHeight + 8

            Text {
                id: sizeText
                anchors.centerIn: parent
                text: Math.round(selection.width) + " × " + Math.round(selection.height)
                color: ThemeColors.foreground
                font.pixelSize: 12
            }
        }

        // Top-center usage hint.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 40
            radius: 8
            color: Qt.alpha(ThemeColors.panelColorAlt, 0.95)
            border.width: 1
            border.color: ThemeColors.border
            width: hintText.implicitWidth + 28
            height: hintText.implicitHeight + 14

            Text {
                id: hintText
                anchors.centerIn: parent
                text: I18n.t("Drag to select a region · Esc or right-click to cancel")
                color: ThemeColors.foreground
                font.pixelSize: 14
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: Qt.CrossCursor
            property point startPos
            property bool dragging: false

            onPressed: (mouse) => {
                if (mouse.button === Qt.RightButton) {
                    overlay.canceled()
                    return
                }
                startPos = Qt.point(mouse.x, mouse.y)
                dragging = true
                selection.x = mouse.x
                selection.y = mouse.y
                selection.width = 0
                selection.height = 0
            }

            onPositionChanged: (mouse) => {
                if (!dragging) {
                    return
                }
                selection.x = Math.max(0, Math.min(startPos.x, mouse.x))
                selection.y = Math.max(0, Math.min(startPos.y, mouse.y))
                selection.width = Math.min(content.width, Math.max(startPos.x, mouse.x)) - selection.x
                selection.height = Math.min(content.height, Math.max(startPos.y, mouse.y)) - selection.y
            }

            onReleased: (mouse) => {
                if (mouse.button !== Qt.LeftButton || !dragging) {
                    return
                }
                dragging = false
                // Too small to be meaningful: reset and keep selecting rather
                // than cancelling on a stray click.
                if (selection.width < 8 || selection.height < 8) {
                    selection.width = 0
                    selection.height = 0
                    return
                }
                // Report the selection in preview-image pixels; the service
                // maps it back onto the original grab.
                const sx = overlay.previewWidth / content.width
                const sy = overlay.previewHeight / content.height
                overlay.accepted(overlay.screenIndex,
                                 Math.round(selection.x * sx),
                                 Math.round(selection.y * sy),
                                 Math.round(selection.width * sx),
                                 Math.round(selection.height * sy))
            }
        }
    }
}

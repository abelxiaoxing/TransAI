import QtQuick
import QtQuick.Controls
import ".."

// Round icon-less button (settings back button). Extracted from
// SettingView.qml (was the inline RoundIconButton component).
Button {
    id: control

    readonly property color accentHover: ThemeColors.accentHover
    readonly property color foreground: ThemeColors.foreground
    readonly property color border: ThemeColors.border
    readonly property color borderActive: ThemeColors.borderActive

    property string label: ""
    hoverEnabled: true
    scale: down ? 0.94 : 1.0

    Behavior on scale { NumberAnimation { duration: 90 } }

    background: Rectangle {
        radius: control.width / 2
        color: control.down ? ThemeColors.controlDown
             : (control.hovered ? ThemeColors.controlHover : ThemeColors.controlIdle)
        border.width: 1
        border.color: control.hovered ? control.borderActive : control.border
        Behavior on color { ColorAnimation { duration: 160 } }
    }

    contentItem: Text {
        text: control.label
        color: control.hovered ? control.accentHover : control.foreground
        font.pixelSize: 24
        font.weight: Font.Light
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}

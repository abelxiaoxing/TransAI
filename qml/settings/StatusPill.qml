import QtQuick
import ".."

// Small status chip (colored dot + label) used in the settings header.
// Extracted from SettingView.qml (was the inline StatusPill component).
Rectangle {
    id: pill

    readonly property color accent: ThemeColors.accent
    readonly property color mutedForeground: ThemeColors.mutedForeground
    readonly property int fontSizeLabel: 13

    property string label: ""
    property color dotColor: accent
    // 宽度留 5px 余量：Text implicitWidth 与可用宽度完全相等时会触发 elide
    implicitWidth: pillText.implicitWidth + 32
    width: implicitWidth
    height: 22
    radius: 11
    color: ThemeColors.chipBg
    border.width: 1
    border.color: ThemeColors.chipBorder

    Rectangle {
        width: 6
        height: 6
        radius: 3
        anchors.left: parent.left
        anchors.leftMargin: 9
        anchors.verticalCenter: parent.verticalCenter
        color: pill.dotColor
    }

    Text {
        id: pillText
        anchors.left: parent.left
        anchors.leftMargin: 19
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: pill.label
        color: pill.mutedForeground
        font.pixelSize: pill.fontSizeLabel
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }
}

import QtQuick
import ".."

// 左侧标签 + 右侧控件的单行布局；控件列从 fieldColumnX 对齐到右缘。
// Extracted from SettingView.qml (was the inline SettingsRow component).
Item {
    id: settingsRow

    readonly property color foreground: ThemeColors.foreground
    readonly property int fontSizeLarge: 17
    readonly property real fieldColumnX: 120

    property string label: ""
    default property alias contentData: contentArea.data
    width: parent ? parent.width : 0
    height: Math.max(rowLabel.implicitHeight, contentArea.childrenRect.height)

    Text {
        id: rowLabel
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: settingsRow.label
        color: settingsRow.foreground
        font.pixelSize: settingsRow.fontSizeLarge
        font.weight: Font.DemiBold
        font.letterSpacing: 0.6
    }

    Item {
        id: contentArea
        anchors.left: parent.left
        anchors.leftMargin: Math.max(settingsRow.fieldColumnX, rowLabel.implicitWidth + 16)
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: childrenRect.height
    }
}

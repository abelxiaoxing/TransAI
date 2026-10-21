import QtQuick
import ".."

// Settings-page card container: title row (with an optional inline control
// area on the right) above a vertically-stacked body. Extracted from
// SettingView.qml (was the inline SettingsCard component); visual constants
// stay identical.
Rectangle {
    id: card

    readonly property color accent: ThemeColors.accent
    readonly property color accentHover: ThemeColors.accentHover
    readonly property color accentPurple: ThemeColors.accentPurple
    readonly property color foreground: ThemeColors.foreground
    readonly property color cardColor: ThemeColors.cardColor
    readonly property int fontSizeLarge: 17
    readonly property real radiusLarge: 16
    // X of the control column: header controls align to the same column as
    // SettingsRow contents.
    readonly property real fieldColumnX: 120

    width: parent ? parent.width : 0
    height: cardColumn.height + 28
    radius: radiusLarge
    color: cardColor
    border.width: 1
    border.color: Qt.alpha(accent, 0.14)

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: parent.radius - 1
        color: "transparent"
        border.width: 1
        border.color: ThemeColors.hairline
    }

    default property alias contentData: bodyColumn.data
    property string title: ""

    Column {
        id: cardColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 10

        Item {
            id: cardHeader
            width: parent.width
            // 标题行只放标题：所有控件都走 SettingsRow（左侧大写标签 + 控件列），
            // 不再有标题行右侧的内联控件槽。
            height: Math.max(18, titleText.implicitHeight)

            Rectangle {
                width: 3
                height: 18
                radius: 1.5
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                gradient: Gradient {
                    GradientStop { position: 0.0; color: card.accentHover }
                    GradientStop { position: 1.0; color: card.accentPurple }
                }
            }

            Text {
                id: titleText
                anchors.left: parent.left
                anchors.leftMargin: 13
                anchors.verticalCenter: parent.verticalCenter
                text: card.title
                color: card.foreground
                font.pixelSize: card.fontSizeLarge
                font.weight: Font.DemiBold
            }
        }

        Column {
            id: bodyColumn
            width: parent.width
            spacing: 10
            height: childrenRect.height
        }
    }
}

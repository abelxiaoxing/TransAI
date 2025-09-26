import QtQuick
import QtQuick.Controls

Button {
    property alias textColor: t.color
    id:btn
    width:t.width + 20
    height:t.height + 10

    // 主题颜色定义
    readonly property color backgroundColor: "#1E1E1E"
    readonly property color foreground: "#D4D4D4"
    readonly property color accent: "#4EC9B0"
    readonly property color accentHover: "#5ED9C0"
    readonly property color border: "#3E3E42"
    readonly property color backgroundSecondary: "#252526"
    readonly property int fontSizeNormal: 14
    readonly property real radius: 8
    background: Rectangle{
        anchors.fill: parent
        opacity: btn.pressed?0.5:0
        color:"transparent"
    }
    contentItem: Item{
        Text {
            id:t
            text: btn.text
            font: btn.font
            anchors.centerIn: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color:foreground

        }
    }
    font.bold: true
    font.capitalization: Font.MixedCase

}

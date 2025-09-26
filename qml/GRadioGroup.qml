import QtQuick
import QtQuick.Controls
import "."

Item {
    id:root
    height:25
    width:300

    property int currentIndex:0
    property alias model: repeater.model

    // 主题颜色定义
    readonly property color backgroundColor: "#1E1E1E"
    readonly property color foreground: "#D4D4D4"
    readonly property color accent: "#4EC9B0"
    readonly property color accentHover: "#5ED9C0"
    readonly property color border: "#3E3E42"
    readonly property color backgroundSecondary: "#252526"
    readonly property int fontSizeNormal: 14
    readonly property real radius: 8

    //选中框
    Rectangle{
        id:selector
        height: root.height
        width: 110
        anchors.top:parent.top
        anchors.verticalCenter: parent.verticalCenter
        x: 0
        radius:8
        gradient: Gradient {
            GradientStop { id:gstart; position: 0.0;    color: accent  }
            GradientStop { id:gend; position: 1.0;    color: accentHover }
        }
        NumberAnimation on x {
            id:ani
            to: 0
            duration:200
            onRunningChanged:{
                if(!running){
                    if(ani.to === repeater.itemAt(0).x){
                        selector.width = repeater.itemAt(0).width
                        repeater.itemAt(0).textColor = backgroundColor
                        repeater.itemAt(1).textColor = accent
                        repeater.itemAt(2).textColor = accent
                    }else if(ani.to === repeater.itemAt(1).x){
                        selector.width = repeater.itemAt(1).width

                        repeater.itemAt(1).textColor = backgroundColor
                        repeater.itemAt(0).textColor = accent
                        repeater.itemAt(2).textColor = accent
                    }else if(ani.to === repeater.itemAt(2).x){
                        selector.width = repeater.itemAt(2).width

                        repeater.itemAt(2).textColor = backgroundColor
                        repeater.itemAt(0).textColor = accent
                        repeater.itemAt(1).textColor = accent
                    }
                }
            }
        }

    }

    Row{
        anchors.fill: parent
        Repeater {
            id:repeater
            GRadioButton {
                anchors.verticalCenter: parent.verticalCenter
                text:modelData
                onClicked: {
                   ani.to = repeater.itemAt(index).x
                   ani.start()
                   currentIndex = index
               }
            }

        }
    }

}

import QtQuick
import QtQuick.Controls
Item {
    id: root
    width: 100
    height:100

    property url normalUrl      //常规状态下的图片路径
    property url hoveredUrl     //悬浮
    property url pressedUrl     //按下

    property alias imageItem: img           //直接别名导出Image实例，外面可以修改其任意属性
    //点击信号
    signal clicked();

    function getSource(){
        return root.enabled
            ? (area.containsPress ? pressedUrl : (area.containsMouse ? hoveredUrl : normalUrl))
            : normalUrl
    }

    Image {
        id: img
        anchors.fill: parent
        smooth: true
        sourceSize.width: root.width * 2 //一定要比image大，否则会失真，太大也会失真
        sourceSize.height: root.height * 2
        //默认按鼠标状态选取不同的图片
        source: getSource()
        // Declarative opacity keeps the binding intact (no imperative writes
        // that would break the enabled-dim behaviour).
        opacity: root.enabled
            ? (area.containsPress ? 0.6 : (area.containsMouse ? 0.8 : 1.0))
            : 0.5
        Behavior on opacity {
            NumberAnimation { duration: 90 }
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent;
        hoverEnabled: root.enabled;
        onClicked: root.clicked()

        cursorShape: Qt.PointingHandCursor
        preventStealing: true
    }
}

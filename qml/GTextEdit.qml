import QtQuick
import QtQuick.Controls

Flickable {
    id: flick

    property bool autoScroll:false
    property alias text: textedit.text
    property alias readOnly: textedit.readOnly
    property alias textedit: textedit

    // 主题颜色定义
    readonly property color foreground: "#D4D4D4"
    readonly property color accentHover: "#5ED9C0"
    readonly property color background: "#1E1E1E"
    readonly property int fontSizeNormal: 14
    readonly property int spacing: 8


    contentWidth: width;
    contentHeight: textedit.height
    flickableDirection: Flickable.VerticalFlick
    clip: true
    function scrollToBottom() {
        if(flick.contentHeight - flick.height >= 0)
            flick.contentY = flick.contentHeight - flick.height
    }
    ScrollBar.vertical:ScrollBar {
        id: scrollbar
//        anchors {
//            top: parent.top
//            right: parent.right
//            bottom: parent.bottom
//        }
        width:10
        orientation: Qt.Vertical
        size: flick.height / flick.contentHeight

    }

    onContentHeightChanged: {
        if(autoScroll)
            flick.scrollToBottom()
    }
    TextEdit{
        id: textedit
        padding: flick.spacing
        wrapMode: TextEdit.WrapAnywhere
        width:flick.width
        font.pixelSize: flick.fontSizeNormal
        selectByMouse:true
        selectByKeyboard:true
        color:flick.foreground
        selectionColor: flick.accentHover
        selectedTextColor: flick.background
    }
}

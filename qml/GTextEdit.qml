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
    readonly property int fontSizeNormal: 20
    readonly property int spacing: 8


    contentWidth: width
    // Ensure the Flickable content area is at least as tall as the visible editor.
    // Otherwise only the text's implicit height (usually the first line at the top)
    // is clickable, and clicks in the empty lower part fall through to outer items.
    contentHeight: Math.max(textedit.height, height)
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

    // 全局鼠标区域解决焦点问题
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true

        onClicked: {
            if (mouse.button === Qt.LeftButton) {
                // 计算点击位置对应的文本位置，处理空文本情况
                var pos = textedit.length > 0 ?
                    textedit.positionAt(mouse.x, mouse.y) : 0;

                // 设置焦点和光标位置
                textedit.forceActiveFocus();
                textedit.cursorPosition = pos;
                mouse.accepted = true;
            } else {
                mouse.accepted = false;
            }
        }
    }

    TextEdit{
        id: textedit
        padding: flick.spacing
        wrapMode: TextEdit.WrapAnywhere
        width:flick.width
        height: Math.max(implicitHeight, flick.height)
        font.pixelSize: flick.fontSizeNormal
        selectByMouse:true
        selectByKeyboard:true
        color:flick.foreground
        selectionColor: flick.accentHover
        selectedTextColor: flick.background
    }
}

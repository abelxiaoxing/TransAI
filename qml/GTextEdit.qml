import QtQuick
import QtQuick.Controls

Flickable {
    id: flick

    property bool autoScroll:false
    property alias text: textedit.text
    property alias readOnly: textedit.readOnly
    property alias textedit: textedit

    // 主题颜色（统一来自 ThemeColors 单例）
    readonly property color foreground: ThemeColors.foreground
    readonly property color accentHover: ThemeColors.accentHover
    readonly property color background: ThemeColors.background
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
        width:10
        orientation: Qt.Vertical
        // Guard against division by zero while content height is being measured.
        size: flick.contentHeight > 0 ? Math.min(1, flick.height / flick.contentHeight) : 1
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
        selectByKeyboard:true
        color:flick.foreground
        selectionColor: flick.accentHover
        selectedTextColor: flick.background
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TransAI


import QtQuick.Controls.Material
import "."

Item {
    id:root
    property bool pinned: false
    property alias inputText: inputArea.text
    property bool inputHasFocus: inputArea.textedit.activeFocus
    property bool resultHasFocus: result.textedit.activeFocus
    property string statusText: "Ready"
    property bool statusBusy: false
    signal settingClicked;

    // 主题颜色（统一来自 ThemeColors 单例）
    readonly property color backgroundColor: ThemeColors.background
    readonly property color foreground: ThemeColors.foreground
    readonly property color accent: ThemeColors.accent
    readonly property color accentHover: ThemeColors.accentHover
    readonly property color accentPurple: ThemeColors.accentPurple
    readonly property color border: ThemeColors.border
    readonly property color borderActive: ThemeColors.borderActive
    readonly property color backgroundSecondary: ThemeColors.backgroundSecondary
    readonly property color panelColor: ThemeColors.panelColor
    readonly property color panelColorAlt: ThemeColors.panelColorAlt
    readonly property color fieldColor: ThemeColors.fieldColor
    readonly property color error: ThemeColors.error
    readonly property color success: ThemeColors.success
    readonly property color warning: ThemeColors.warning
    readonly property color infoBg: ThemeColors.infoBg
    readonly property color warningBg: ThemeColors.warningBg
    readonly property color infoText: ThemeColors.infoText
    readonly property color warningText: ThemeColors.warningText
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeNormal: 16
    readonly property int fontSizeLarge: 18
    readonly property real radius: 8
    function startTrans(source){
        if (inputArea.text.length === 0) {
            return
        }
        translationService.translateFromMap({
            "text": inputArea.text,
            "targetLanguage": langSelector.currentText,
            "mode": getMode(),
            "source": source || "manual",
            "provider": setting.provider,
            "apiServer": setting.apiServer,
            "apiKey": setting.apiKey,
            "model": setting.model,
            "thinkingMode": setting.thinkingMode
        })
    }

    function showSelectionError(message) {
        translationService.abort()
        result.text = message
        statusBusy = false
        statusText = "Selection unavailable"
    }

    function getMode(){
        return 0
    }

    Component.onCompleted: {
        // 应用启动时默认置顶
        mainWindow.flags = mainWindow.flags | Qt.WindowStaysOnTopHint
        tItem.state = "yes"
        pinned = true
        statusText = "Ready"
    }

    Rectangle {
        id:appBackground
        anchors.fill: parent
        z:0
        color: backgroundColor
        gradient: Gradient {
            GradientStop { position: 0.0; color: ThemeColors.gradientTop }
            GradientStop { position: 0.52; color: backgroundColor }
            GradientStop { position: 1.0; color: ThemeColors.gradientBottom }
        }
    }

    Rectangle {
        z:1
        width: 260
        height: 260
        radius: 130
        x: -120
        y: -90
        color: Qt.rgba(0.39, 0.96, 0.84, 0.12)
        border.color: Qt.rgba(0.39, 0.96, 0.84, 0.18)
        border.width: 1
    }

    Rectangle {
        z:1
        width: 220
        height: 220
        radius: 110
        anchors.right: parent.right
        anchors.rightMargin: -110
        anchors.top: parent.top
        anchors.topMargin: 110
        color: Qt.rgba(0.49, 0.36, 1.0, 0.10)
        border.color: Qt.rgba(0.49, 0.36, 1.0, 0.16)
        border.width: 1
    }

    Item{
        z:2
        id:header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right:parent.right
        anchors.margins: 15
        opacity: 1
        height:26

        Text {
            id: titleText
            text: "Quick Translate"
            color: root.accent
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: root.fontSizeLarge + 2
            font.weight: Font.DemiBold
            opacity: 0.85
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: -10
            height: 1
            color: Qt.rgba(100.0 / 255.0, 212.0 / 255.0, 255.0 / 255.0, 0.12)
        }


        IconButton{
            id:settingBtn
            z:1
            width: 26
            height:26
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 58
            normalUrl:"qrc:///res/setting1.png"
            hoveredUrl:"qrc:///res/setting1.png"
            pressedUrl:"qrc:///res/setting2.png"
            onClicked: {
                settingClicked();
            }
        }

        Rectangle {
            id: statusPill
            anchors.top: parent.top
            anchors.right: parent.right
            height: 18
            radius: 9
            color: root.statusBusy ? root.warningBg : root.infoBg
            border.width: 1
            border.color: root.statusBusy ? root.warningText : root.infoText
            opacity: 1
            Behavior on color {
                ColorAnimation {
                    duration: 80
                }
            }
            Behavior on border.color {
                ColorAnimation {
                    duration: 80
                }
            }
            Behavior on opacity {
                NumberAnimation {
                    duration: 80
                }
            }

            Text {
                id: statusTextItem
                anchors.centerIn: parent
                text: root.statusText
                color: root.statusBusy ? root.warningText : root.infoText
                font.pixelSize: root.fontSizeSmall
                font.weight: Font.DemiBold
                Behavior on color {
                    ColorAnimation {
                        duration: 80
                    }
                }
            }

            implicitWidth: statusTextItem.implicitWidth + 16
            implicitHeight: 18
        }

    }

    Item{
        z:2
        id:inputItem
        anchors.margins: 10
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        clip:true
        opacity: 1
        Rectangle {
            id: inputPanelBg
            radius: root.radius
            color: root.panelColor
            border.width : 1
            border.color: root.inputHasFocus ? root.borderActive : Qt.rgba(0.24, 0.4, 0.57, 0.35)
            Behavior on border.color {
                ColorAnimation {
                    duration: 80
                    easing.type: Easing.OutQuad
                }
            }
            anchors.fill: parent
        }
        Rectangle {
            anchors.fill: parent
            radius: root.radius
            color: "transparent"
            border.width: 1
            border.color: root.accent
            opacity: root.inputHasFocus ? 0.35 : 0
            Behavior on opacity {
                NumberAnimation {
                    duration: 80
                }
            }
        }
        GTextEdit{
            id:inputArea
            anchors.fill: parent
            autoScroll:false
            textedit.focus : true
        }
        // Height follows the window so the layout stays usable after resizing;
        // the Behavior keeps the (formerly animated) intro transition subtle.
        Behavior on height {
            NumberAnimation {
                duration: 80
            }
        }
        height: root.height / 3
    }

    Item{
        id:tItem
        opacity: 1
        width: 22
        height:22
        anchors.top: inputItem.bottom
        anchors.topMargin: 10
        anchors.right:inputItem.right
        state:"no"
        Image{
            id:tyes
            anchors.fill: parent
            source:"qrc:///res/thumbtack_yes.png"
            visible:tItem.state === "yes"
            MouseArea{
                anchors.fill: parent
                onClicked: {
                    mainWindow.flags = mainWindow.flags & ~Qt.WindowStaysOnTopHint
                    tItem.state = "no"
                    pinned = false

                }
            }
        }
        Image{
            id:tyno
            anchors.fill: parent
            source:"qrc:///res/thumbtack_no.png"
            visible:tItem.state === "no"
            MouseArea{
                anchors.fill: parent
                onClicked: {
                    mainWindow.flags = mainWindow.flags |Qt.WindowStaysOnTopHint
                    tItem.state = "yes"
                    pinned = true

                }
            }
        }
    }

    Text{
        id:indictor
        text:"Translated"
        anchors.left: inputItem.left
        anchors.top:inputItem.bottom
        opacity: 1
        font.bold: true
        color:root.accent
        anchors.topMargin: 28
        font.pixelSize: root.fontSizeNormal
        font.weight: Font.DemiBold
        z:2
    }

    Item{
        id:resultPanel
        z:2
        anchors.left: inputItem.left
        anchors.right: inputItem.right
        anchors.top:indictor.bottom
        anchors.topMargin: 8
        anchors.bottom: langSelector.top
        anchors.bottomMargin: 10
        opacity: 1
        clip:true
        Rectangle {
            id: resultPanelBg
            anchors.fill: parent
            radius: root.radius
            color: root.panelColorAlt
            border.width: 1
            border.color: root.resultHasFocus ? root.borderActive : root.border
            Behavior on border.color {
                ColorAnimation {
                    duration: 80
                    easing.type: Easing.OutQuad
                }
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: root.radius
            color: "transparent"
            border.width: 1
            border.color: root.accent
            opacity: root.resultHasFocus ? 0.35 : 0
            Behavior on opacity {
                NumberAnimation {
                    duration: 80
                }
            }
        }
        GTextEdit{
            id:result
            anchors.fill: parent
            anchors.margins: 1
            autoScroll:true
            readOnly:true
        }
    }

    Button{
        id:transBtn
        z:2
        anchors.bottom: parent.bottom
        anchors.right:parent.right
        anchors.margins:10
        opacity: 1
        text:(Qt.platform.os == "macos" || Qt.platform.os == "osx")?"Translate ⌘Enter":"Translate (Ctrl+Enter)"
        font.capitalization: Font.MixedCase
        enabled:inputArea.text.length > 0
        onClicked: {
            root.startTrans("manual")
        }
        height:50
        background: Rectangle {
            color: transBtn.enabled
                ? (transBtn.pressed ? Qt.darker(root.accent, 1.35) : (transBtn.hovered ? root.accentHover : root.accent))
                : Qt.rgba(101.0 / 255.0, 244.0 / 255.0, 214.0 / 255.0, 0.35)
            radius: root.radius
            border.width: 1
            border.color: root.borderActive
            Behavior on color {
                ColorAnimation {
                    duration: 80
                    easing.type: Easing.OutQuad
                }
            }
        }
        contentItem: Text {
            text: transBtn.text
            color: transBtn.enabled ? "#0F1721" : "#A3B0C0"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: root.fontSizeNormal
            font.weight: Font.Medium
        }
    }
    BusyIndicator {
        anchors.verticalCenter: stopBtn.verticalCenter
        anchors.right:stopBtn.left
        anchors.rightMargin: 10
        running: translationService.isRequesting
        visible:translationService.isRequesting
        width:transBtn.height - 10
        height:width
    }
    Button{
        id:stopBtn
        visible:false
        z:2
        anchors.bottom: parent.bottom
        anchors.right:parent.right
        anchors.margins:10
        text:"stop"
        onClicked: {
            translationService.abort()
        }
        height:50
        background: Rectangle {
            id: stopBtnBg
            color: root.error
            radius: root.radius
            border.width: 1
            border.color: Qt.darker(root.error, 1.2)
            Behavior on color {
                ColorAnimation {
                    duration: 80
                    easing.type: Easing.OutQuad
                }
            }
            Behavior on border.color {
                ColorAnimation {
                    duration: 80
                    easing.type: Easing.OutQuad
                }
            }
        }
        contentItem: Text {
            text: stopBtn.text
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: root.fontSizeNormal
        }
    }

    ComboBox {
        id:langSelector
        z:2
        anchors.bottom: parent.bottom
        anchors.left:parent.left
        anchors.margins:10
        opacity: 1
        currentIndex: 0
        model:["简体中文","繁体中文", "English", "Japanese", "German", "Korean", "Español", "français"]
        onCurrentTextChanged: {
            // TranslationService receives targetLanguage per request.
        }
        height:40

        // 应用主题样式
        background: Rectangle {
            color: langSelector.hovered || langSelector.popup.opened || langSelector.down ? Qt.darker(root.fieldColor, 1.08) : root.fieldColor
            radius: root.radius
            border.width: 1
            border.color: langSelector.hovered || langSelector.popup.opened ? root.borderActive : root.border
            Behavior on color {
                ColorAnimation {
                    duration: 80
                    easing.type: Easing.OutQuad
                }
            }
            Behavior on border.color {
                ColorAnimation {
                    duration: 80
                    easing.type: Easing.OutQuad
                }
            }
        }

        contentItem: Text {
            text: langSelector.currentText
            color: root.foreground
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            rightPadding: langSelector.indicator.width + 10
            font.pixelSize: root.fontSizeNormal
        }

        delegate: ItemDelegate {
            width: langSelector.width
            height: 40
            contentItem: Text {
                text: modelData
                color: root.foreground
                font.pixelSize: root.fontSizeNormal
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                leftPadding: 10
                rightPadding: 10
            }
            background: Rectangle {
                color: langSelector.highlightedIndex === index ? root.accentHover : "transparent"
                radius: root.radius
            }
        }

        indicator: Canvas {
            id: canvas
            x: langSelector.width - width - 10
            y: (langSelector.height - height) / 2
            width: 12
            height: 8
            contextType: "2d"

            onPaint: {
                context.reset()
                context.moveTo(0, 0)
                context.lineTo(width, 0)
                context.lineTo(width / 2, height)
                context.closePath()
                context.fillStyle = root.foreground
                context.fill()
            }
        }

        popup: Popup {
            y: langSelector.height
            width: langSelector.width
            implicitHeight: contentItem.implicitHeight
            padding: 1

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: langSelector.delegateModel
                currentIndex: langSelector.highlightedIndex

                ScrollIndicator.vertical: ScrollIndicator { }
            }

            background: Rectangle {
                color: root.panelColorAlt
                radius: root.radius
                border.color: root.border
                border.width: 1
            }
        }
    }

    TranslationService{
        id:translationService
        onTranslationStarted: {
            result.text = ""
            root.statusBusy = true
            root.statusText = "Translating..."
        }
        onResponseDataChanged: {
            if (responseError === "") {
                result.text = responseData
            }
        }
        onResponseErrorChanged: {
            if (responseError !== "") {
                result.text = responseError
                root.statusBusy = false
                root.statusText = "Error"
            }
        }
        onIsRequestingChanged: {
            if(isRequesting){
                root.statusBusy = true
                root.statusText = "Translating..."
                transBtn.visible = false
                stopBtn.visible = true
            }else{
                root.statusBusy = false
                root.statusText = responseError !== "" ? "Error" : "Ready"
                transBtn.visible = true
                stopBtn.visible = false
            }
        }
        onTranslationStopped: {
            // Keep the partial result visible, but distinguish it from Ready.
            root.statusText = "Stopped"
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Controller


import QtQuick.Controls.Material
import "."

Item {
    id:root
    property bool pinned: false
    property alias inputText: inputArea.text
    signal settingClicked;

    // 主题颜色定义
    readonly property color backgroundColor: "#090D13"
    readonly property color foreground: "#E8F3F7"
    readonly property color accent: "#65F4D6"
    readonly property color accentHover: "#8CFFE8"
    readonly property color accentPurple: "#7C5CFF"
    readonly property color border: "#243445"
    readonly property color borderActive: "#65F4D6"
    readonly property color backgroundSecondary: "#121A24"
    readonly property color panelColor: "#141F2A"
    readonly property color panelColorAlt: "#101720"
    readonly property color fieldColor: "#0D141D"
    readonly property color error: "#F48771"
    readonly property color success: "#4EC9B0"
    readonly property color warning: "#FFD166"
    readonly property int fontSizeNormal: 14
    readonly property int fontSizeLarge: 16
    readonly property real radius: 8
    function startTrans(){
        if(inputArea.text.length > 0 && transBtn.visible)
            transBtn.clicked()
    }

    function getMode(){
        return 0
    }

    Component.onCompleted: {
        inputAni.to = root.height /3
        inputAni.start()
        // 应用启动时默认置顶
        mainWindow.flags = mainWindow.flags | Qt.WindowStaysOnTopHint
        tItem.state = "yes"
        pinned = true
    }

    Rectangle {
        id:appBackground
        anchors.fill: parent
        z:0
        color: backgroundColor
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#101827" }
            GradientStop { position: 0.52; color: backgroundColor }
            GradientStop { position: 1.0; color: "#070A10" }
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

        height:30

        Text {
            text: "Translation"
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            font.bold: true
            color: root.foreground
            font.pixelSize: root.fontSizeLarge
        }


        IconButton{
            id:settingBtn
            width: 18
            height:18
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            normalUrl:"qrc:///res/setting1.png"
            hoveredUrl:"qrc:///res/setting1.png"
            pressedUrl:"qrc:///res/setting2.png"
            onClicked: {
                settingClicked();
            }
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
        Rectangle {
            radius: root.radius
            color: root.panelColor
            border.width : 1
            border.color: Qt.rgba(0.24, 0.4, 0.57, 0.35)
            anchors.fill: parent
        }
        GTextEdit{
            id:inputArea
            anchors.fill: parent
            autoScroll:false
            textedit.focus : true
        }
        NumberAnimation on height {
            id:inputAni
            to: root.height/3
            duration:200
        }
    }

    Item{
        id:tItem
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
                    mainWindow.flags = mainWindow.flags & (0xFFFFFFFF ^ Qt.WindowStaysOnTopHint)
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
        clip:true
        Rectangle {
            anchors.fill: parent
            radius: root.radius
            color: root.panelColorAlt
            border.width: 1
            border.color: root.border
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
        text:(Qt.platform.os == "macos" || Qt.platform.os == "osx")?"Translate ⌘Enter":"Translate (Ctrl+Enter)"
        font.capitalization: Font.MixedCase
        enabled:inputArea.text.length > 0
        onClicked: {
            api.sendMessage(inputArea.text, getMode())
        }
        height:50
        background: Rectangle {
            color: "transparent"
            radius: root.radius
            border.width: 1
            border.color: root.borderActive
            gradient: Gradient {
                GradientStop { position: 0.0; color: root.accent }
                GradientStop { position: 1.0; color: Qt.darker(root.accent, 1.35) }
            }
        }
        contentItem: Text {
            text: transBtn.text
            color: "#0F1721"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: root.fontSizeNormal
        }
    }
    BusyIndicator {
        anchors.verticalCenter: stopBtn.verticalCenter
        anchors.right:stopBtn.left
        anchors.rightMargin: 10
        running: api.isRequesting
        visible:api.isRequesting
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
            api.abort()
        }
        height:50
        background: Rectangle {
            color: root.error
            radius: root.radius
            border.width: 1
            border.color: Qt.darker(root.error, 1.2)
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
        currentIndex: 0
        model:["简体中文","繁体中文", "English", "Japanse", "German", "Korean", "Español", "français"]
        onCurrentTextChanged: {
            api.transToLang = currentText
        }
        height:40

        // 应用主题样式
        background: Rectangle {
            color: root.fieldColor
            radius: root.radius
            border.width: 1
            border.color: root.borderActive
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

    APIController{
        id:api
        onResponseDataChanged: {
            result.text = responseData
        }
        onResponseErrorChanged: {
            if(responseError != ""){
                result.text = responseError + ":\n" + result.text
            }
        }
        onIsRequestingChanged: {
            if(isRequesting){
                transBtn.visible = false
                stopBtn.visible = true
            }else{
                transBtn.visible = true
                stopBtn.visible = false
            }
        }
        Component.onCompleted: {
            api.apiKey = setting.apiKey
            api.model = setting.model
            api.apiServer = setting.apiServer
            api.provider = setting.provider
        }
    }
    Connections{
        target: setting
        function onApiServerChanged(){
            api.apiServer = setting.apiServer
        }
        function onApiKeyChanged(){
            api.apiKey = setting.apiKey
        }
        function onModelChanged(){
            api.model = setting.model
        }
        function onProviderChanged(){
            api.provider = setting.provider
        }
    }
}

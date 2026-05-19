import QtQuick
import QtQuick.Controls
import Updater
import Controller

Item {
    id: root
    signal backClicked;


    property bool lock:false
    property string selectedProvider: "openai"
    readonly property bool isOllamaProvider: selectedProvider === "ollama"

    // 主题颜色定义
    readonly property color backgroundColor: "#1E1E1E"
    readonly property color foreground: "#D4D4D4"
    readonly property color accent: "#4EC9B0"
    readonly property color accentHover: "#5ED9C0"
    readonly property color border: "#3E3E42"
    readonly property color backgroundSecondary: "#252526"
    readonly property int fontSizeNormal: 14
    readonly property int fontSizeLarge: 16
    readonly property real radius: 8
    

    function providerFromIndex(index) {
        return index === 1 ? "ollama" : "openai"
    }

    function providerIndex(provider) {
        return provider === "ollama" ? 1 : 0
    }

    function applyProviderDefaults() {
        var server = serverInput.text.trim()
        if (isOllamaProvider && (server === "" || server === "https://api.openai.com")) {
            serverInput.text = "http://localhost:11434/v1/chat/completions"
        } else if (!isOllamaProvider && (server === "" || server === "http://localhost:11434/v1/chat/completions")) {
            serverInput.text = "https://api.openai.com"
        }
    }

    function reload(){
        setting.loadConfig()
        lock = true
        selectedProvider = setting.provider === "ollama" ? "ollama" : "openai"
        providerCombo.currentIndex = providerIndex(selectedProvider)
        keyInput.text = setting.apiKey
        serverInput.text = setting.apiServer
        shortcutText.text = setting.shortCut
        modelInput.text = setting.model

        lock = false
        Qt.callLater(function() {
            if (selectedProvider === "ollama" || keyInput.text.trim().length >= 10) {
                detectModels()
            }
        })

    }

    function saveConfig(){
        if(lock){
            return
        }
        setting.apiServer = serverInput.text.trim()
        setting.apiKey = keyInput.text
        setting.shortCut = shortcutText.text
        setting.model = modelInput.text.trim()
        setting.provider = selectedProvider
        setting.updateConfig()
    }

    function detectModels(){
        modelDetector.detectModels(serverInput.text.trim(), keyInput.text, selectedProvider)
    }
    MouseArea {
        anchors.fill: parent
        onClicked: {
            shortcutRect.focus = false;
            if(shortcutText.text.length > 0){
                hotkey.setShortcut(shortcutText.text)
            }
        }
    }



    Item{
        id:header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right:parent.right
        anchors.margins: 15
        height:30


        IconButton{
            width: 20
            height:20
            anchors.left: parent.left
            anchors.top: parent.top
            normalUrl:"qrc:///res/back1.png"
            hoveredUrl:"qrc:///res/back1.png"
            pressedUrl:"qrc:///res/back2.png"
            onClicked: {
                backClicked();
            }
        }

    }
    Text{
        id:providerText
        anchors.left: header.left
        anchors.top:header.bottom
        anchors.topMargin: 10
        text:"Provider"
        font.bold: true
        color: accent
    }

    Item {
        id:providerItem
        anchors.left: header.left
        anchors.right: header.right
        anchors.top:providerText.bottom
        anchors.topMargin: 10
        height:40
        ComboBox {
            id: providerCombo
            width: parent.width
            height: parent.height
            model: ["OpenAI", "Ollama"]
            currentIndex: 0
            font.pixelSize: fontSizeNormal

            onCurrentIndexChanged: {
                selectedProvider = providerFromIndex(currentIndex)
                if (!lock) {
                    applyProviderDefaults()
                    saveConfig()
                }
            }

            background: Rectangle {
                color: backgroundSecondary
                radius: root.radius
                border.width: 1
                border.color: border
            }

            contentItem: Text {
                text: providerCombo.displayText
                color: foreground
                font.pixelSize: fontSizeNormal
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                leftPadding: 10
                rightPadding: providerCombo.indicator.width + 12
            }

            indicator: Canvas {
                id: providerIndicator
                x: providerCombo.width - width - 12
                y: (providerCombo.height - height) / 2
                width: 12
                height: 8
                contextType: "2d"

                Connections {
                    target: providerCombo
                    function onPressedChanged() { providerIndicator.requestPaint() }
                }

                onPaint: {
                    context.reset()
                    context.moveTo(0, 0)
                    context.lineTo(width, 0)
                    context.lineTo(width / 2, height)
                    context.closePath()
                    context.fillStyle = providerCombo.pressed ? accentHover : foreground
                    context.fill()
                }
            }

            delegate: ItemDelegate {
                width: providerCombo.width
                height: 36
                highlighted: providerCombo.highlightedIndex === index

                contentItem: Text {
                    text: modelData
                    color: providerCombo.currentIndex === index ? accent : foreground
                    font.pixelSize: fontSizeNormal
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    leftPadding: 10
                    rightPadding: 10
                }
                background: Rectangle {
                    color: providerCombo.highlightedIndex === index ? Qt.darker(backgroundSecondary, 1.15) : backgroundSecondary
                    radius: root.radius
                }
            }

            popup: Popup {
                y: providerCombo.height + 2
                width: providerCombo.width
                implicitHeight: contentItem.implicitHeight
                padding: 1

                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight
                    model: providerCombo.popup.visible ? providerCombo.delegateModel : null
                    currentIndex: providerCombo.highlightedIndex
                    ScrollIndicator.vertical: ScrollIndicator { }
                }

                background: Rectangle {
                    color: backgroundSecondary
                    radius: root.radius
                    border.width: 1
                    border.color: border
                }
            }
        }
    }

    Text{
        id:serverText
        anchors.left: header.left
        anchors.top:providerItem.bottom
        anchors.topMargin: 20
        text:"API Server"
        font.bold: true
        color: accent
    }

    Item {
        id:serverItem
        anchors.left: header.left
        anchors.right: header.right
        anchors.top:serverText.bottom
        anchors.topMargin: 10
        height:30
        Rectangle {
            color: backgroundSecondary
            anchors.fill: parent
            radius: radius
        }

        TextInput {
            id:serverInput
            anchors.fill: parent
            padding:7
            text: "https://api.openai.com"
            color: foreground
            font.pixelSize: fontSizeNormal
            onTextChanged: {
                saveConfig()
            }
        }
    }

    Text{
        id:apiText
        anchors.left: header.left
        anchors.top:serverItem.bottom
        anchors.topMargin: 20
        text:"API Key"
        font.bold: true
        color: accent
        visible: !isOllamaProvider
    }

    ScrollView {
        id:keyInputScroll
        anchors.left: header.left
        anchors.right: header.right
        anchors.top:apiText.bottom
        anchors.topMargin: 10
        height: visible ? 80 : 0
        visible: !isOllamaProvider
        contentWidth: width
        contentHeight: keyInput.contentHeight + 20
        ScrollBar.vertical: ScrollBar {
           width:(parent.contentHeight >= parent.height)?10:0
           height:parent.height
           anchors.right: parent.right
           policy: ScrollBar.AlwaysOn
       }
        TextArea{
            id:keyInput
            height:80
            font.pixelSize: fontSizeNormal
            y:20
            color: foreground
            wrapMode: Text.WrapAnywhere
            onTextChanged:{
                saveConfig()
            }
            background: Rectangle{
                color: backgroundSecondary
                radius: radius
            }

        }
    }

    Text{
        id:modelText
        anchors.left: header.left
        anchors.top:isOllamaProvider ? serverItem.bottom : keyInputScroll.bottom
        anchors.topMargin: 20
        text:"Model"
        font.bold: true
        color: accent
    }

    Item {
        id:modelItem
        anchors.left: header.left
        anchors.right: header.right
        anchors.top:modelText.bottom
        anchors.topMargin: 10
        height:30
        Rectangle {
            color: backgroundSecondary
            anchors.fill: parent
            radius: radius
        }

        TextInput {
            id:modelInput
            anchors.left: parent.left
            anchors.right: detectModelBtn.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            padding:7
            text: "gpt-3.5-turbo"
            color: foreground
            font.pixelSize: fontSizeNormal
            onTextChanged: {
                saveConfig()
            }
            onActiveFocusChanged: {
                if (activeFocus && modelDetector.availableModels.length > 0) {
                    modelPopup.open()
                }
            }
        }

        Button {
            id: detectModelBtn
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 88
            text: modelDetector.isDetectingModels ? "Detecting" : "Detect"
            enabled: !modelDetector.isDetectingModels
            font.capitalization: Font.MixedCase
            font.pixelSize: fontSizeNormal
            onClicked: detectModels()
            background: Rectangle {
                color: detectModelBtn.enabled ? accent : border
                radius: root.radius
            }
            contentItem: Text {
                text: detectModelBtn.text
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: fontSizeNormal
            }
        }

        Popup {
            id: modelPopup
            y: modelItem.height + 2
            width: modelItem.width
            implicitHeight: Math.min(modelList.contentHeight + 2, 180)
            padding: 1
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

            contentItem: ListView {
                id: modelList
                clip: true
                implicitHeight: Math.min(contentHeight, 180)
                model: modelDetector.availableModels
                ScrollIndicator.vertical: ScrollIndicator { }
                delegate: ItemDelegate {
                    width: modelList.width
                    height: 36
                    contentItem: Text {
                        text: modelData
                        color: modelInput.text === modelData ? accent : foreground
                        font.pixelSize: fontSizeNormal
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        leftPadding: 10
                        rightPadding: 10
                    }
                    background: Rectangle {
                        color: hovered ? Qt.darker(backgroundSecondary, 1.15) : backgroundSecondary
                    }
                    onClicked: {
                        modelInput.text = modelData
                        modelPopup.close()
                        saveConfig()
                    }
                }
            }

            background: Rectangle {
                color: backgroundSecondary
                radius: root.radius
                border.width: 1
                border.color: border
            }
        }
    }

    Text{
        id:modelDetectTip
        anchors.left: header.left
        anchors.right: header.right
        anchors.top:modelItem.bottom
        anchors.topMargin: 6
        text: modelDetector.modelDetectError !== "" ? modelDetector.modelDetectError : (modelDetector.availableModels.length > 0 ? (modelDetector.availableModels.length + " models detected, click input to choose") : "")
        color: modelDetector.modelDetectError !== "" ? "#F48771" : foreground
        font.pixelSize: 12
        elide: Text.ElideRight
    }

    Text{
        id:shortCutText
        anchors.left: header.left
        anchors.top:modelDetectTip.bottom
        anchors.topMargin: 20
        text:"Shortcut"
        font.bold: true
        color: accent
    }

    Item {
        id:shortcutItem
        anchors.left: header.left
        anchors.top:shortCutText.bottom
        anchors.topMargin: 10
        width:100
        height:30
        Rectangle {
            id:shortcutRect
            color: backgroundSecondary
            anchors.fill: parent
            radius: radius
            border.width:1
            border.color: border
            onActiveFocusChanged: {
            }

            onFocusChanged: {
                if(focus){
                    border.color = "green"
                    shortcutRect.forceActiveFocus()
                    shortcutText.text = ""
                    hotkey.setShortcut("")
                }else{
                    border.color = color
                }

            }

            Text{
                id:shortcutText
                anchors.centerIn: parent
                text:""
                color: foreground
                font.pixelSize: fontSizeNormal
                onTextChanged: {
                    saveConfig()
                }
            }

            Keys.onPressed:(event)=> {
                if(!shortcutRect.focus){
                    return
                }

                shortcutText.text = ""
                var valid = false
                var haveCtrl = false


                if(Qt.platform.os === "macos" || Qt.platform.os === "osx"){
                   if (event.modifiers & Qt.ControlModifier) {
                       shortcutText.text = "Ctrl+"
                       haveCtrl = true
                   }
                   if (event.modifiers & Qt.MetaModifier) {
                       shortcutText.text = "Meta+"
                       haveCtrl = true
                   }
                }else{
                   if (event.modifiers & Qt.ControlModifier) {
                       shortcutText.text = "Ctrl+"
                       haveCtrl = true
                   }
                }

                if (event.modifiers & Qt.AltModifier) {
                   shortcutText.text = "Alt+"
                    haveCtrl = true
                }
                if (event.modifiers & Qt.ShiftModifier) {
                   shortcutText.text = "Shift+"
                    haveCtrl = true
                }

                if(shortCutText.text.length > 0){
                    switch(event.key){
                        case Qt.Key_F1: shortcutText.text = "F1"; valid = true; break;
                        case Qt.Key_F2: shortcutText.text = "F2";valid = true; break;
                        case Qt.Key_F3: shortcutText.text = "F3";valid = true;  break;
                        case Qt.Key_F4: shortcutText.text = "F4"; valid = true; break;
                        case Qt.Key_F5: shortcutText.text = "F5"; valid = true; break;
                        case Qt.Key_F6: shortcutText.text = "F6";valid = true;  break;
                        case Qt.Key_F7: shortcutText.text = "F7"; valid = true; break;
                        case Qt.Key_F8: shortcutText.text = "F8"; valid = true; break;
                        case Qt.Key_F9: shortcutText.text = "F9"; valid = true; break;
                        case Qt.Key_F10: shortcutText.text = "F10"; valid = true; break;
                        case Qt.Key_F11: shortcutText.text = "F11"; valid = true; break;
                        case Qt.Key_F12: shortcutText.text = "F12"; valid = true; break;
                    }
                    if(event.key >= Qt.Key_0  && event.key <= Qt.Key_9 ){
                        if(haveCtrl){
                            shortcutText.text += String.fromCharCode(event.key)
                            valid = true
                        }
                    }else if(event.key >= Qt.Key_A  && event.key <= Qt.Key_Z ){
                        if(haveCtrl){
                            shortcutText.text += String.fromCharCode(event.key)
                            valid = true
                        }
                    }
                }

                if(valid){
                    shortcutRect.focus = false;
                    if(shortcutText.text.length > 0){
                        if(hotkey.setShortcut(shortcutText.text) == false){
                            shortcutRect.focus = false;
                            shortcutText.text = ""
                        }
                    }
                }


            }

        }

        MouseArea{
            anchors.fill: parent
            onClicked: {
                shortcutRect.focus = true
            }
        }



    }


    Text{
        id:about
        anchors.left: header.left
        anchors.top:shortcutItem.bottom
        anchors.topMargin: 20
        text:"About"
        font.bold: true
        color: accent
    }


    Column{
        spacing:10
        anchors.top:about.bottom
        anchors.topMargin: 15
        anchors.left: header.left
        anchors.right: header.right
        Image{
            id:icon
            anchors.horizontalCenter: parent.horizontalCenter
            source:"qrc:///res/logo/logo.ico"
            height:40
            width:40
            MouseArea{
                anchors.fill: parent
                onClicked: {
                    Qt.openUrlExternally("https://github.com/JesseGuoX/GPT-Translator")
                }
            }
        }

        Text{
            id:version
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 15
            text:"Current version:" +  Qt.application.version
            font.bold: true
            font.pixelSize: 12
            color: accent
        }
        Rectangle{
            anchors.horizontalCenter: parent.horizontalCenter
            height:checkBtn.height
            width:parent.width
            color: "transparent"
            Button{
                id:checkBtn
                text:"check update"
                font.capitalization: Font.MixedCase
                height: 40
                anchors.horizontalCenter: parent.horizontalCenter
                background: Rectangle {
                    color: checkBtn.down ? Qt.darker(accent, 1.15) : accent
                    radius: root.radius
                    border.width: 1
                    border.color: accent
                }
                contentItem: Text {
                    text: checkBtn.text
                    color: backgroundColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: fontSizeNormal
                    font.capitalization: Font.MixedCase
                }
                onClicked: {
                    updater.check()
                }
            }
            BusyIndicator {
                anchors.verticalCenter: checkBtn.verticalCenter
                anchors.left:checkBtn.right
                anchors.leftMargin: 10
                running: updater.isRequesting
                visible:updater.isRequesting
                width:checkBtn.height - 10
                height:width
            }
        }


        Text{
            id:linkText
            anchors.horizontalCenter: parent.horizontalCenter

            visible:!updater.isRequesting
            text: "<u><a href='" + "https://www.google.com" + "'>" + updater.requestResult + "</a></u>"
            color: accent
            linkColor: accentHover
            onLinkActivated: Qt.openUrlExternally(updater.updateLink)
        }
        TextArea{
            width:parent.width
            visible:!updater.isRequesting
            text:updater.releaseNote
            readOnly: true
            color: foreground
            font.pixelSize: fontSizeNormal
            wrapMode: Text.WrapAnywhere
            y:30
            background: Rectangle {
                color: backgroundSecondary
                radius: radius
            }
        }
    }



    APIController{
        id:modelDetector
        onAvailableModelsChanged: {
            if (availableModels.length > 0) {
                modelPopup.open()
            }
        }
    }

    APIUpdater{
        id:updater
        onIsRequestingChanged: {
            if(isRequesting){
                checkBtn.enabled = false
            }else{
                checkBtn.enabled = true
                if(updater.updateLink.length > 0){
                    linkText.text = "<u><a  href='" + updater.updateLink + "'>" + updater.requestResult + "</a></u>"
                }else{
                    linkText.text = requestResult
                }


            }
        }
    }

}

import QtQuick
import QtQuick.Controls
import Controller

Item {
    id: root
    signal backClicked;

    property bool lock:false
    property string selectedProvider: "openai"
    property string selectedModel: "gpt-3.5-turbo"
    property bool openModelPopupAfterDetect: false
    readonly property bool isOllamaProvider: selectedProvider === "ollama"
    readonly property string ollamaApiServer: "http://localhost:11434"

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
        if (isOllamaProvider) {
            serverInput.text = ollamaApiServer
        } else if (server === "" || server === ollamaApiServer || server === ollamaApiServer + "/v1/chat/completions") {
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
        selectedModel = setting.model.trim().length > 0 ? setting.model : "gpt-3.5-turbo"
        openModelPopupAfterDetect = false
        modelPopup.close()
        applyProviderDefaults()

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
        setting.apiServer = isOllamaProvider ? ollamaApiServer : serverInput.text.trim()
        setting.apiKey = keyInput.text
        setting.shortCut = shortcutText.text
        setting.model = selectedModel.trim()
        setting.provider = selectedProvider
        setting.updateConfig()
    }

    function detectModels(){
        modelDetector.detectModels(isOllamaProvider ? ollamaApiServer : serverInput.text.trim(), keyInput.text, selectedProvider)
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

    Flickable {
        id: flickable
        anchors.fill: parent
        anchors.margins: 15
        contentHeight: contentColumn.height + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0

            // 返回按钮
            Item {
                width: parent.width
                height: 30
                IconButton {
                    width: 20
                    height: 20
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    normalUrl: "qrc:///res/back1.png"
                    hoveredUrl: "qrc:///res/back1.png"
                    pressedUrl: "qrc:///res/back2.png"
                    onClicked: backClicked()
                }
            }

            // ---- Provider ----
            SectionTitle { text: "Provider" }
            Item {
                width: parent.width
                height: 36
                ComboBox {
                    id: providerCombo
                    anchors.fill: parent
                    model: ["OpenAI", "Ollama"]
                    currentIndex: 0
                    font.pixelSize: fontSizeNormal

                    onCurrentIndexChanged: {
                        selectedProvider = providerFromIndex(currentIndex)
                        openModelPopupAfterDetect = false
                        modelPopup.close()
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

            Separator {}

            // ---- API Server ----
            SectionTitle {
                text: "API Server"
                visible: !isOllamaProvider
            }
            Item {
                width: parent.width
                height: visible ? 36 : 0
                visible: !isOllamaProvider
                Rectangle {
                    color: backgroundSecondary
                    anchors.fill: parent
                    radius: radius
                }
                TextInput {
                    id: serverInput
                    anchors.fill: parent
                    padding: 10
                    text: "https://api.openai.com"
                    color: foreground
                    font.pixelSize: fontSizeNormal
                    verticalAlignment: Text.AlignVCenter
                    onTextChanged: saveConfig()
                }
            }

            Separator { visible: !isOllamaProvider }

            // ---- API Key ----
            SectionTitle {
                text: "API Key"
                visible: !isOllamaProvider
            }
            ScrollView {
                width: parent.width
                height: visible ? 72 : 0
                visible: !isOllamaProvider
                contentWidth: width
                contentHeight: keyInput.contentHeight + 20
                ScrollBar.vertical: ScrollBar {
                    width: (parent.contentHeight >= parent.height) ? 8 : 0
                    height: parent.height
                    anchors.right: parent.right
                    policy: ScrollBar.AlwaysOn
                }
                TextArea {
                    id: keyInput
                    height: 72
                    font.pixelSize: fontSizeNormal
                    color: foreground
                    wrapMode: Text.WrapAnywhere
                    onTextChanged: saveConfig()
                    background: Rectangle {
                        color: backgroundSecondary
                        radius: radius
                    }
                }
            }

            Separator { visible: !isOllamaProvider }

            // ---- Model ----
            SectionTitle { text: "Model" }
            Item {
                width: parent.width
                height: 36
                Button {
                    id: modelSelectBtn
                    anchors.left: parent.left
                    anchors.right: detectModelBtn.left
                    anchors.rightMargin: 8
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    hoverEnabled: true
                    font.capitalization: Font.MixedCase
                    onClicked: {
                        if (modelDetector.availableModels.length > 0) {
                            modelPopup.open()
                        } else if (!modelDetector.isDetectingModels) {
                            openModelPopupAfterDetect = true
                            detectModels()
                        }
                    }
                    background: Rectangle {
                        color: modelSelectBtn.down ? Qt.darker(backgroundSecondary, 1.2)
                                                   : (modelSelectBtn.hovered || modelPopup.opened ? Qt.darker(backgroundSecondary, 1.08) : backgroundSecondary)
                        radius: root.radius
                        border.width: 1
                        border.color: modelPopup.opened ? accent : (modelSelectBtn.hovered ? accentHover : border)
                    }
                    contentItem: Item {
                        Text {
                            anchors.left: parent.left
                            anchors.right: arrowText.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 10
                            anchors.rightMargin: 8
                            text: selectedModel.length > 0 ? selectedModel
                                                           : (modelDetector.availableModels.length > 0 ? "Select model" : "Detect models first")
                            color: selectedModel.length > 0 ? foreground : Qt.darker(foreground, 1.35)
                            font.pixelSize: fontSizeNormal
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        Text {
                            id: arrowText
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelPopup.opened ? "▴" : "▾"
                            color: modelSelectBtn.hovered || modelPopup.opened ? accentHover : foreground
                            font.pixelSize: 16
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                Button {
                    id: detectModelBtn
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 80
                    text: modelDetector.isDetectingModels ? "Detecting" : "Detect"
                    enabled: !modelDetector.isDetectingModels
                    font.capitalization: Font.MixedCase
                    font.pixelSize: fontSizeNormal
                    onClicked: {
                        openModelPopupAfterDetect = false
                        detectModels()
                    }
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
                    x: modelSelectBtn.x
                    y: parent.height + 2
                    width: modelSelectBtn.width
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
                                color: selectedModel === modelData ? accent : foreground
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
                                selectedModel = modelData
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

            Item {
                width: parent.width
                height: modelDetectTip.visible ? modelDetectTip.implicitHeight + 4 : 0
                Text {
                    id: modelDetectTip
                    anchors.bottom: parent.bottom
                    width: parent.width
                    text: modelDetector.modelDetectError !== "" ? modelDetector.modelDetectError
                                                                  : (modelDetector.availableModels.length > 0 ? (modelDetector.availableModels.length + " models detected, click Model to choose") : "")
                    color: modelDetector.modelDetectError !== "" ? "#F48771" : foreground
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    visible: text.length > 0
                }
            }

            Separator {}

            // ---- Shortcut ----
            SectionTitle { text: "Shortcut" }
            Item {
                width: parent.width
                height: 36
                Rectangle {
                    id: shortcutRect
                    color: backgroundSecondary
                    anchors.fill: parent
                    radius: radius
                    border.width: 1
                    border.color: border

                    onFocusChanged: {
                        if (focus) {
                            border.color = accent
                            shortcutRect.forceActiveFocus()
                            shortcutText.text = ""
                            hotkey.setShortcut("")
                        } else {
                            border.color = border
                        }
                    }

                    Text {
                        id: shortcutText
                        anchors.centerIn: parent
                        text: ""
                        color: foreground
                        font.pixelSize: fontSizeNormal
                        onTextChanged: saveConfig()
                    }

                    Keys.onPressed: (event) => {
                        if (!shortcutRect.focus) return

                        shortcutText.text = ""
                        var valid = false
                        var haveCtrl = false

                        if (Qt.platform.os === "macos" || Qt.platform.os === "osx") {
                            if (event.modifiers & Qt.ControlModifier) {
                                shortcutText.text = "Ctrl+"
                                haveCtrl = true
                            }
                            if (event.modifiers & Qt.MetaModifier) {
                                shortcutText.text = "Meta+"
                                haveCtrl = true
                            }
                        } else {
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

                        if (shortcutText.text.length > 0) {
                            switch (event.key) {
                                case Qt.Key_F1: shortcutText.text = "F1"; valid = true; break;
                                case Qt.Key_F2: shortcutText.text = "F2"; valid = true; break;
                                case Qt.Key_F3: shortcutText.text = "F3"; valid = true; break;
                                case Qt.Key_F4: shortcutText.text = "F4"; valid = true; break;
                                case Qt.Key_F5: shortcutText.text = "F5"; valid = true; break;
                                case Qt.Key_F6: shortcutText.text = "F6"; valid = true; break;
                                case Qt.Key_F7: shortcutText.text = "F7"; valid = true; break;
                                case Qt.Key_F8: shortcutText.text = "F8"; valid = true; break;
                                case Qt.Key_F9: shortcutText.text = "F9"; valid = true; break;
                                case Qt.Key_F10: shortcutText.text = "F10"; valid = true; break;
                                case Qt.Key_F11: shortcutText.text = "F11"; valid = true; break;
                                case Qt.Key_F12: shortcutText.text = "F12"; valid = true; break;
                            }
                            if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
                                if (haveCtrl) {
                                    shortcutText.text += String.fromCharCode(event.key)
                                    valid = true
                                }
                            } else if (event.key >= Qt.Key_A && event.key <= Qt.Key_Z) {
                                if (haveCtrl) {
                                    shortcutText.text += String.fromCharCode(event.key)
                                    valid = true
                                }
                            }
                        }

                        if (valid) {
                            shortcutRect.focus = false
                            if (shortcutText.text.length > 0) {
                                if (hotkey.setShortcut(shortcutText.text) == false) {
                                    shortcutRect.focus = false
                                    shortcutText.text = ""
                                }
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: shortcutRect.focus = true
                }
            }

            Separator {}

            // 底部极简版本信息
            Item {
                width: parent.width
                height: 24
                Text {
                    anchors.centerIn: parent
                    text: "GPT Translator  v" + Qt.application.version
                    color: Qt.darker(foreground, 1.6)
                    font.pixelSize: 11
                }
            }
        }
    }

    // 可复用的标题组件
    component SectionTitle: Item {
        width: parent.width
        height: sectionText.height + 24
        property alias text: sectionText.text
        Text {
            id: sectionText
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            font.bold: true
            color: accent
            font.pixelSize: 13
        }
    }

    // 可复用的分隔线
    component Separator: Item {
        width: parent.width
        height: visible ? 17 : 0
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: border
            opacity: 0.5
        }
    }

    APIController {
        id: modelDetector
        onAvailableModelsChanged: {
            if (openModelPopupAfterDetect && availableModels.length > 0) {
                modelPopup.open()
                openModelPopupAfterDetect = false
            }
        }
        onIsDetectingModelsChanged: {
            if (!isDetectingModels && availableModels.length === 0) {
                openModelPopupAfterDetect = false
            }
        }
    }

}

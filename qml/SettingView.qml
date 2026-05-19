import QtQuick
import QtQuick.Controls
import Controller

Item {
    id: root
    signal backClicked

    property bool lock: false
    property string selectedProvider: "openai"
    property string selectedModel: "gpt-3.5-turbo"
    property bool openModelPopupAfterDetect: false
    property string shortcutBeforeRecording: ""
    property bool shortcutRecordingAccepted: false
    readonly property bool isOllamaProvider: selectedProvider === "ollama"
    readonly property string ollamaApiServer: "http://localhost:11434"

    // Futuristic dark design tokens
    readonly property color backgroundColor: "#090D13"
    readonly property color backgroundSecondary: "#121A24"
    readonly property color panelColor: "#141F2A"
    readonly property color panelColorAlt: "#101720"
    readonly property color fieldColor: "#0D141D"
    readonly property color foreground: "#E8F3F7"
    readonly property color mutedForeground: "#89A2B2"
    readonly property color subtleForeground: "#5F7484"
    readonly property color accent: "#65F4D6"
    readonly property color accentHover: "#8CFFE8"
    readonly property color accentPurple: "#7C5CFF"
    readonly property color border: "#243445"
    readonly property color borderActive: "#65F4D6"
    readonly property color error: "#FF7A90"
    readonly property color success: "#65F4D6"
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeNormal: 14
    readonly property int fontSizeLarge: 17
    readonly property int fontSizeTitle: 24
    readonly property real radius: 12
    readonly property real radiusLarge: 18

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

    function reload() {
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

    function saveConfig() {
        if (lock) {
            return
        }
        setting.apiServer = isOllamaProvider ? ollamaApiServer : serverInput.text.trim()
        setting.apiKey = keyInput.text
        setting.shortCut = shortcutText.text
        setting.model = selectedModel.trim()
        setting.provider = selectedProvider
        setting.updateConfig()
    }

    function detectModels() {
        modelDetector.detectModels(isOllamaProvider ? ollamaApiServer : serverInput.text.trim(), keyInput.text, selectedProvider)
    }

    function keyNameFromEvent(event) {
        if (event.key >= Qt.Key_A && event.key <= Qt.Key_Z) {
            return String.fromCharCode(event.key)
        }
        if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            return String.fromCharCode(event.key)
        }
        switch (event.key) {
        case Qt.Key_F1: return "F1"
        case Qt.Key_F2: return "F2"
        case Qt.Key_F3: return "F3"
        case Qt.Key_F4: return "F4"
        case Qt.Key_F5: return "F5"
        case Qt.Key_F6: return "F6"
        case Qt.Key_F7: return "F7"
        case Qt.Key_F8: return "F8"
        case Qt.Key_F9: return "F9"
        case Qt.Key_F10: return "F10"
        case Qt.Key_F11: return "F11"
        case Qt.Key_F12: return "F12"
        default: return ""
        }
    }

    function shortcutFromEvent(event) {
        var parts = []
        if (event.modifiers & Qt.ControlModifier) {
            parts.push("Ctrl")
        }
        if (event.modifiers & Qt.MetaModifier) {
            parts.push("Meta")
        }
        if (event.modifiers & Qt.AltModifier) {
            parts.push("Alt")
        }
        if (event.modifiers & Qt.ShiftModifier) {
            parts.push("Shift")
        }

        var keyName = keyNameFromEvent(event)
        if (keyName.length === 0) {
            return ""
        }

        var isFunctionKey = keyName.charAt(0) === "F"
        if (parts.length === 0 && !isFunctionKey) {
            return ""
        }
        return parts.length > 0 ? parts.join("+") + "+" + keyName : keyName
    }

    Rectangle {
        anchors.fill: parent
        color: backgroundColor
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#101827" }
            GradientStop { position: 0.52; color: "#090D13" }
            GradientStop { position: 1.0; color: "#070A10" }
        }
    }

    // Subtle neon ambience without expensive shader effects.
    Rectangle {
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
        width: 220
        height: 220
        radius: 110
        anchors.right: parent.right
        anchors.rightMargin: -110
        anchors.top: parent.top
        anchors.topMargin: 90
        color: Qt.rgba(0.49, 0.36, 1.0, 0.10)
        border.color: Qt.rgba(0.49, 0.36, 1.0, 0.16)
        border.width: 1
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            shortcutRect.focus = false
            if (shortcutText.text.length > 0) {
                hotkey.setShortcut(shortcutText.text)
            }
        }
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        anchors.margins: 18
        contentHeight: contentColumn.height + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 14

            Item {
                id: header
                width: parent.width
                height: 78

                RoundIconButton {
                    id: backButton
                    anchors.left: parent.left
                    anchors.top: parent.top
                    width: 40
                    height: 40
                    label: "‹"
                    tooltip: "Back"
                    onClicked: backClicked()
                }

                Column {
                    anchors.left: backButton.right
                    anchors.leftMargin: 12
                    anchors.right: parent.right
                    anchors.verticalCenter: backButton.verticalCenter
                    spacing: 2

                    Text {
                        text: "Settings"
                        color: foreground
                        font.pixelSize: fontSizeTitle
                        font.bold: true
                        elide: Text.ElideRight
                        width: parent.width
                    }

                    Text {
                        text: "Neural translation control center"
                        color: mutedForeground
                        font.pixelSize: fontSizeSmall
                        elide: Text.ElideRight
                        width: parent.width
                    }
                }

                Row {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    spacing: 8

                    StatusPill {
                        label: selectedProvider === "ollama" ? "LOCAL" : "CLOUD"
                        dotColor: selectedProvider === "ollama" ? accentPurple : accent
                    }

                    StatusPill {
                        label: selectedModel.length > 0 ? selectedModel : "NO MODEL"
                        dotColor: modelDetector.modelDetectError !== "" ? error : accent
                        width: Math.min(190, implicitWidth)
                    }
                }
            }

            SettingsCard {
                title: "AI Provider"
                subtitle: "Connect TransAI to your preferred inference endpoint."

                Text {
                    width: parent.width
                    text: "Provider"
                    color: mutedForeground
                    font.pixelSize: fontSizeSmall
                    font.weight: Font.DemiBold
                }

                ComboBox {
                    id: providerCombo
                    width: parent.width
                    height: 44
                    model: ["OpenAI", "Ollama"]
                    currentIndex: 0
                    font.pixelSize: fontSizeNormal
                    hoverEnabled: true

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
                        color: providerCombo.hovered || providerCombo.popup.opened ? "#152332" : fieldColor
                        radius: root.radius
                        border.width: 1
                        border.color: providerCombo.popup.opened ? borderActive : (providerCombo.hovered ? Qt.rgba(0.39, 0.96, 0.84, 0.55) : border)
                        Behavior on color { ColorAnimation { duration: 160 } }
                    }

                    contentItem: Text {
                        text: providerCombo.displayText
                        color: foreground
                        font.pixelSize: fontSizeNormal
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        leftPadding: 14
                        rightPadding: 40
                    }

                    indicator: Text {
                        x: providerCombo.width - width - 14
                        y: (providerCombo.height - height) / 2
                        text: providerCombo.popup.opened ? "▴" : "▾"
                        color: providerCombo.hovered || providerCombo.popup.opened ? accentHover : mutedForeground
                        font.pixelSize: 16
                    }

                    delegate: ItemDelegate {
                        width: providerCombo.width
                        height: 38
                        highlighted: providerCombo.highlightedIndex === index

                        contentItem: Text {
                            text: modelData
                            color: providerCombo.currentIndex === index ? accent : foreground
                            font.pixelSize: fontSizeNormal
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            leftPadding: 12
                            rightPadding: 12
                        }

                        background: Rectangle {
                            color: providerCombo.highlightedIndex === index ? "#1A2A3A" : panelColorAlt
                            radius: 8
                        }
                    }

                    popup: Popup {
                        y: providerCombo.height + 6
                        width: providerCombo.width
                        implicitHeight: contentItem.implicitHeight + 2
                        padding: 1

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: providerCombo.popup.visible ? providerCombo.delegateModel : null
                            currentIndex: providerCombo.highlightedIndex
                            ScrollIndicator.vertical: ScrollIndicator { }
                        }

                        background: Rectangle {
                            color: panelColorAlt
                            radius: root.radius
                            border.width: 1
                            border.color: borderActive
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 7
                    visible: !isOllamaProvider
                    height: visible ? implicitHeight : 0

                    Text {
                        width: parent.width
                        text: "API Server"
                        color: mutedForeground
                        font.pixelSize: fontSizeSmall
                        font.weight: Font.DemiBold
                    }

                    TextField {
                        id: serverInput
                        width: parent.width
                        height: 44
                        text: "https://api.openai.com"
                        color: foreground
                        placeholderText: "https://api.openai.com"
                        placeholderTextColor: subtleForeground
                        font.pixelSize: fontSizeNormal
                        leftPadding: 14
                        rightPadding: 14
                        verticalAlignment: TextInput.AlignVCenter
                        selectByMouse: true
                        onTextChanged: saveConfig()

                        background: Rectangle {
                            color: serverInput.activeFocus ? "#152332" : fieldColor
                            radius: root.radius
                            border.width: 1
                            border.color: serverInput.activeFocus ? borderActive : (serverInput.hovered ? Qt.rgba(0.39, 0.96, 0.84, 0.55) : border)
                            Behavior on color { ColorAnimation { duration: 160 } }
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 7
                    visible: !isOllamaProvider
                    height: visible ? implicitHeight : 0

                    Row {
                        width: parent.width
                        height: 16

                        Text {
                            text: "API Key"
                            color: mutedForeground
                            font.pixelSize: fontSizeSmall
                            font.weight: Font.DemiBold
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: keyInput.text.trim().length > 0 ? "saved locally" : "required"
                            color: keyInput.text.trim().length > 0 ? accent : subtleForeground
                            font.pixelSize: 11
                        }
                    }

                    ScrollView {
                        id: keyScroll
                        width: parent.width
                        height: 86
                        clip: true
                        contentWidth: availableWidth
                        ScrollBar.vertical: ScrollBar {
                            policy: keyInput.contentHeight > keyScroll.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                            width: 7
                        }

                        TextArea {
                            id: keyInput
                            width: keyScroll.availableWidth
                            height: keyScroll.height
                            color: foreground
                            placeholderText: "sk-..."
                            placeholderTextColor: subtleForeground
                            font.pixelSize: fontSizeNormal
                            wrapMode: Text.WrapAnywhere
                            selectByMouse: true
                            topPadding: 10
                            bottomPadding: 10
                            leftPadding: 14
                            rightPadding: 14
                            onTextChanged: saveConfig()

                            background: Rectangle {
                                color: keyInput.activeFocus ? "#152332" : fieldColor
                                radius: root.radius
                                border.width: 1
                                border.color: keyInput.activeFocus ? borderActive : (keyInput.hovered ? Qt.rgba(0.39, 0.96, 0.84, 0.55) : border)
                                Behavior on color { ColorAnimation { duration: 160 } }
                            }
                        }
                    }
                }
            }

            SettingsCard {
                title: "Model Router"
                subtitle: "Discover available models and bind one for translation."

                Text {
                    width: parent.width
                    text: "Model"
                    color: mutedForeground
                    font.pixelSize: fontSizeSmall
                    font.weight: Font.DemiBold
                }

                Item {
                    id: modelRow
                    width: parent.width
                    height: 44

                    TextField {
                        id: modelInput
                        anchors.left: parent.left
                        anchors.right: detectModelBtn.left
                        anchors.rightMargin: 10
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        text: selectedModel
                        color: foreground
                        placeholderText: modelDetector.availableModels.length > 0 ? "Select model or type custom model" : "Detect models first"
                        placeholderTextColor: subtleForeground
                        font.pixelSize: fontSizeNormal
                        leftPadding: 14
                        rightPadding: 34
                        selectByMouse: true
                        onTextChanged: {
                            selectedModel = text.trim()
                            if (!lock) {
                                saveConfig()
                            }
                        }

                        background: Rectangle {
                            color: modelInput.activeFocus || modelPopup.opened ? "#152332" : fieldColor
                            radius: root.radius
                            border.width: 1
                            border.color: modelPopup.opened ? borderActive : (modelInput.hovered || modelInput.activeFocus ? Qt.rgba(0.39, 0.96, 0.84, 0.55) : border)
                            Behavior on color { ColorAnimation { duration: 160 } }
                        }

                        MouseArea {
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            anchors.rightMargin: 0
                            width: 36
                            acceptedButtons: Qt.LeftButton
                            z: 2
                            onClicked: {
                                if (modelDetector.availableModels.length > 0 && !modelPopup.opened) {
                                    modelPopup.open()
                                } else if (!modelDetector.isDetectingModels && !modelPopup.opened) {
                                    openModelPopupAfterDetect = true
                                    detectModels()
                                }
                            }
                        }

                        Text {
                            id: modelSelectArrow
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelPopup.opened ? "▴" : "▾"
                            color: modelPopup.opened ? accentHover : mutedForeground
                            font.pixelSize: 16
                        }
                    }

                    Button {
                        id: detectModelBtn
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 96
                        text: modelDetector.isDetectingModels ? "Scanning" : "Detect"
                        enabled: !modelDetector.isDetectingModels
                        hoverEnabled: true
                        font.capitalization: Font.MixedCase
                        font.pixelSize: fontSizeNormal
                        scale: down ? 0.97 : 1.0

                        onClicked: {
                            openModelPopupAfterDetect = false
                            detectModels()
                        }

                        Behavior on scale { NumberAnimation { duration: 90 } }

                        background: Rectangle {
                            radius: root.radius
                            color: detectModelBtn.enabled ? (detectModelBtn.hovered ? accentHover : accent) : "#2A3541"
                            border.width: 1
                            border.color: detectModelBtn.enabled ? Qt.rgba(1, 1, 1, 0.18) : border
                            Behavior on color { ColorAnimation { duration: 160 } }
                        }

                        contentItem: Text {
                            text: detectModelBtn.text
                            color: detectModelBtn.enabled ? "#061014" : subtleForeground
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: fontSizeNormal
                            font.weight: Font.DemiBold
                        }
                    }

                    Popup {
                        id: modelPopup
                        x: modelInput.x
                        y: modelInput.height + 6
                        width: modelInput.width
                        implicitHeight: Math.min(modelList.contentHeight + 2, 196)
                        padding: 1
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                        contentItem: ListView {
                            id: modelList
                            clip: true
                            implicitHeight: Math.min(contentHeight, 196)
                            model: modelDetector.availableModels
                            ScrollIndicator.vertical: ScrollIndicator { }

                            delegate: ItemDelegate {
                                width: modelList.width
                                height: 38
                                hoverEnabled: true

                                contentItem: Text {
                                    text: modelData
                                    color: selectedModel === modelData ? accent : foreground
                                    font.pixelSize: fontSizeNormal
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    leftPadding: 12
                                    rightPadding: 12
                                }

                                background: Rectangle {
                                    color: hovered ? "#1A2A3A" : panelColorAlt
                                }

                                onClicked: {
                                    selectedModel = modelData
                                    modelPopup.close()
                                    saveConfig()
                                }
                            }
                        }

                        background: Rectangle {
                            color: panelColorAlt
                            radius: root.radius
                            border.width: 1
                            border.color: borderActive
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: modelDetectTip.visible ? 34 : 0
                    visible: modelDetectTip.text.length > 0
                    radius: 10
                    color: modelDetector.modelDetectError !== "" ? Qt.rgba(1.0, 0.48, 0.56, 0.10) : Qt.rgba(0.39, 0.96, 0.84, 0.09)
                    border.width: 1
                    border.color: modelDetector.modelDetectError !== "" ? Qt.rgba(1.0, 0.48, 0.56, 0.35) : Qt.rgba(0.39, 0.96, 0.84, 0.28)

                    Text {
                        id: modelDetectTip
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        text: modelDetector.modelDetectError !== "" ? modelDetector.modelDetectError
                                                                      : (modelDetector.availableModels.length > 0 ? (modelDetector.availableModels.length + " models detected · click Model to choose") : "")
                        color: modelDetector.modelDetectError !== "" ? error : mutedForeground
                        font.pixelSize: fontSizeSmall
                        elide: Text.ElideRight
                    }
                }
            }

            SettingsCard {
                title: "Global Hotkey"
                subtitle: "Capture selected text from anywhere and translate instantly."

                Text {
                    width: parent.width
                    text: "Shortcut"
                    color: mutedForeground
                    font.pixelSize: fontSizeSmall
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    id: shortcutRect
                    width: parent.width
                    height: 52
                    color: activeFocus ? "#152332" : fieldColor
                    radius: root.radius
                    border.width: 1
                    border.color: activeFocus ? borderActive : border
                    focus: false

                    onActiveFocusChanged: {
                        if (activeFocus) {
                            shortcutBeforeRecording = shortcutText.text
                            shortcutRecordingAccepted = false
                            if (shortcutBeforeRecording.length > 0) {
                                hotkey.setShortcut("")
                            }
                        } else if (!shortcutRecordingAccepted && shortcutBeforeRecording.length > 0) {
                            hotkey.setShortcut(shortcutBeforeRecording)
                        }
                    }

                    Behavior on color { ColorAnimation { duration: 160 } }

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 10

                        Rectangle {
                            width: 24
                            height: 24
                            radius: 12
                            anchors.verticalCenter: parent.verticalCenter
                            color: shortcutRect.activeFocus ? Qt.rgba(0.39, 0.96, 0.84, 0.18) : Qt.rgba(0.54, 0.64, 0.70, 0.12)
                            border.width: 1
                            border.color: shortcutRect.activeFocus ? accent : border

                            Text {
                                anchors.centerIn: parent
                                text: "⌘"
                                color: shortcutRect.activeFocus ? accent : mutedForeground
                                font.pixelSize: 13
                            }
                        }

                        Text {
                            id: shortcutText
                            width: parent.width - 34
                            anchors.verticalCenter: parent.verticalCenter
                            text: ""
                            color: text.length > 0 ? foreground : subtleForeground
                            font.pixelSize: fontSizeNormal
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            onTextChanged: saveConfig()
                        }
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        visible: shortcutText.text.length === 0 && !shortcutRect.activeFocus
                        text: "Click to record"
                        color: subtleForeground
                        font.pixelSize: fontSizeSmall
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        visible: shortcutRect.activeFocus
                        text: "Press keys..."
                        color: accent
                        font.pixelSize: fontSizeSmall
                    }

                    Keys.onPressed: (event) => {
                        if (!shortcutRect.activeFocus) {
                            return
                        }

                        if (event.key === Qt.Key_Escape) {
                            shortcutRect.focus = false
                            event.accepted = true
                            return
                        }

                        var shortcut = shortcutFromEvent(event)
                        if (shortcut.length > 0) {
                            if (hotkey.setShortcut(shortcut) === false) {
                                if (shortcutBeforeRecording.length > 0) {
                                    hotkey.setShortcut(shortcutBeforeRecording)
                                }
                                shortcutRect.focus = false
                                event.accepted = true
                                return
                            }
                            shortcutRecordingAccepted = true
                            shortcutText.text = shortcut
                            shortcutRect.focus = false
                        }
                        event.accepted = true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: shortcutRect.forceActiveFocus()
                    }
                }
            }

            Item {
                width: parent.width
                height: 36

                Text {
                    anchors.centerIn: parent
                    text: "GPT Translator  v" + Qt.application.version
                    color: subtleForeground
                    font.pixelSize: 11
                }
            }
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

    component SettingsCard: Rectangle {
        width: parent ? parent.width : 0
        height: cardColumn.height + 28
        radius: root.radiusLarge
        color: Qt.rgba(0.07, 0.11, 0.16, 0.82)
        border.width: 1
        border.color: Qt.rgba(0.39, 0.96, 0.84, 0.14)

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: parent.radius - 1
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.04)
        }

        default property alias contentData: bodyColumn.data
        property string title: ""
        property string subtitle: ""

        Column {
            id: cardColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 14
            spacing: 14

            Row {
                width: parent.width
                height: Math.max(38, titleColumn.implicitHeight)
                spacing: 10

                Rectangle {
                    width: 4
                    height: 32
                    radius: 2
                    anchors.verticalCenter: parent.verticalCenter
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: accentHover }
                        GradientStop { position: 1.0; color: accentPurple }
                    }
                }

                Column {
                    id: titleColumn
                    width: parent.width - 14
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        width: parent.width
                        text: title
                        color: foreground
                        font.pixelSize: fontSizeLarge
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: subtitle
                        visible: subtitle.length > 0
                        color: mutedForeground
                        font.pixelSize: fontSizeSmall
                        elide: Text.ElideRight
                    }
                }
            }

            Column {
                id: bodyColumn
                width: parent.width
                spacing: 10
            }
        }
    }

    component RoundIconButton: Button {
        id: control
        property string label: ""
        property string tooltip: ""
        hoverEnabled: true
        scale: down ? 0.94 : 1.0

        Behavior on scale { NumberAnimation { duration: 90 } }

        background: Rectangle {
            radius: control.width / 2
            color: control.down ? "#0C1219" : (control.hovered ? "#182838" : Qt.rgba(0.07, 0.11, 0.16, 0.76))
            border.width: 1
            border.color: control.hovered ? borderActive : border
            Behavior on color { ColorAnimation { duration: 160 } }
        }

        contentItem: Text {
            text: control.label
            color: control.hovered ? accentHover : foreground
            font.pixelSize: 28
            font.weight: Font.Light
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component StatusPill: Rectangle {
        property string label: ""
        property color dotColor: accent
        implicitWidth: pillText.implicitWidth + 30
        width: implicitWidth
        height: 24
        radius: 12
        color: Qt.rgba(1, 1, 1, 0.055)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.08)

        Rectangle {
            width: 7
            height: 7
            radius: 4
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            color: dotColor
        }

        Text {
            id: pillText
            anchors.left: parent.left
            anchors.leftMargin: 21
            anchors.right: parent.right
            anchors.rightMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            text: label
            color: mutedForeground
            font.pixelSize: 10
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
    }
}

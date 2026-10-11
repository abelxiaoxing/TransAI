import QtQuick
import QtQuick.Controls
import TransAI

Item {
    id: root
    signal backClicked

    property bool ready: false
    property string selectedProvider: "openai"
    property string selectedModel: "gpt-3.5-turbo"
    property bool openModelPopupAfterDetect: false
    property string autoDetectServer: ""
    property string autoDetectKey: ""
    property string autoDetectProvider: ""
    property string shortcutBeforeRecording: ""
    property bool shortcutRecordingAccepted: false
    property QtObject shortcutRegistrar: null
    readonly property bool isOllamaProvider: selectedProvider === "ollama"
    readonly property string defaultOllamaApiServer: "http://localhost:11434"
    readonly property string defaultOpenAiApiServer: "https://api.openai.com"

    // 主题颜色（统一来自 ThemeColors 单例；字号/圆角保留本页自己的尺度）
    readonly property color backgroundColor: ThemeColors.background
    readonly property color backgroundSecondary: ThemeColors.backgroundSecondary
    readonly property color panelColor: ThemeColors.panelColor
    readonly property color panelColorAlt: ThemeColors.panelColorAlt
    readonly property color fieldColor: ThemeColors.fieldColor
    readonly property color foreground: ThemeColors.foreground
    readonly property color mutedForeground: ThemeColors.mutedForeground
    readonly property color subtleForeground: ThemeColors.subtleForeground
    readonly property color accent: ThemeColors.accent
    readonly property color accentHover: ThemeColors.accentHover
    readonly property color accentPurple: ThemeColors.accentPurple
    readonly property color border: ThemeColors.border
    readonly property color borderActive: ThemeColors.borderActive
    readonly property color error: ThemeColors.error
    readonly property color success: ThemeColors.success
    readonly property int fontSizeSmall: 16
    readonly property int fontSizeNormal: 20
    readonly property int fontSizeLarge: 22
    readonly property int fontSizeTitle: 25
    readonly property real radius: 12
    readonly property real radiusLarge: 18

    function providerFromIndex(index) {
        return index === 1 ? "ollama" : "openai"
    }

    function providerIndex(provider) {
        return provider === "ollama" ? 1 : 0
    }

    function thinkingModeFromIndex(index) {
        const modes = ["auto", "qwen", "deepseek", "openai", "ollama", "manual"]
        return modes[index] || "auto"
    }

    function thinkingIndex(mode) {
        const modes = ["auto", "qwen", "deepseek", "openai", "ollama", "manual"]
        const i = modes.indexOf(mode)
        return i >= 0 ? i : 0
    }

    function normalizedApiServer() {
        return serverInput.text.trim().length > 0 ? serverInput.text.trim() : defaultOpenAiApiServer
    }

    function applyProviderSettings(provider) {
        var isOllama = provider === "ollama"
        selectedProvider = isOllama ? "ollama" : "openai"
        openModelPopupAfterDetect = false
        modelPopup.close()

        if (isOllama) {
            serverInput.text = setting.ollamaApiServer.trim().length > 0 ? setting.ollamaApiServer : defaultOllamaApiServer
            selectedModel = setting.ollamaModel.trim().length > 0 ? setting.ollamaModel : ""
        } else {
            serverInput.text = setting.openaiApiServer.trim().length > 0 ? setting.openaiApiServer : defaultOpenAiApiServer
            keyInput.text = setting.openaiApiKey
            selectedModel = setting.openaiModel.trim().length > 0 ? setting.openaiModel : "gpt-3.5-turbo"
        }
        setting.applyProviderConfig(selectedProvider)
    }

    function persistCurrentModel() {
        const model = selectedModel.trim()
        if (selectedProvider === "ollama") {
            setting.ollamaModel = model
        } else {
            setting.openaiModel = model.length > 0 ? model : "gpt-3.5-turbo"
        }
    }

    function reload() {
        ready = false
        setting.loadConfig()
        selectedProvider = setting.provider === "ollama" ? "ollama" : "openai"
        providerCombo.currentIndex = providerIndex(selectedProvider)
        applyProviderSettings(selectedProvider)
        keyInput.text = setting.openaiApiKey
        shortcutText.text = setting.shortCut
        selectedModel = isOllamaProvider ? setting.ollamaModel : setting.openaiModel
        if (selectedModel.trim().length === 0 && !isOllamaProvider) {
            selectedModel = "gpt-3.5-turbo"
        }
        openModelPopupAfterDetect = false
        modelPopup.close()

        ready = true
        Qt.callLater(function() {
            if (selectedProvider === "ollama" || keyInput.text.trim().length >= 10) {
                detectModels()
            }
        })
    }

    function saveConfig() {
        if (!ready) {
            return
        }
        if (isOllamaProvider) {
            setting.ollamaApiServer = serverInput.text.trim().length > 0 ? serverInput.text.trim() : defaultOllamaApiServer
            setting.ollamaModel = selectedModel.trim()
        } else {
            const server = serverInput.text.trim().length > 0 ? serverInput.text.trim() : defaultOpenAiApiServer
            setting.openaiApiServer = server
            setting.openaiApiKey = keyInput.text
            setting.openaiModel = selectedModel.trim().length > 0 ? selectedModel.trim() : "gpt-3.5-turbo"
        }
        setting.shortCut = shortcutText.text
        setting.applyProviderConfig(selectedProvider)
        setting.updateConfig()
    }

    // Debounced save for text inputs: writes at most once per pause in typing
    // instead of once per keystroke.
    function saveConfigSoon() {
        if (!ready) {
            return
        }
        saveDebounceTimer.restart()
    }

    function detectModels() {
        modelDetector.detectModels(isOllamaProvider ? (serverInput.text.trim().length > 0 ? serverInput.text.trim() : defaultOllamaApiServer) : serverInput.text.trim(), keyInput.text, selectedProvider)
    }

    function queueAutoDetectModels() {
        if (!ready) {
            return
        }
        const server = isOllamaProvider ? (serverInput.text.trim().length > 0 ? serverInput.text.trim() : defaultOllamaApiServer) : normalizedApiServer()
        const key = isOllamaProvider ? "" : keyInput.text.trim()
        if (!isOllamaProvider && key.length < 10) {
            return
        }
        if (autoDetectProvider === selectedProvider && autoDetectServer === server && autoDetectKey === key) {
            return
        }
        autoDetectProvider = selectedProvider
        autoDetectServer = server
        autoDetectKey = key
        autoDetectTimer.restart()
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

    AppBackground {
        accentCircleTopMargin: 90
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            shortcutRect.focus = false
            if (shortcutText.text.length > 0 && shortcutRegistrar !== null) {
                shortcutRegistrar.setShortcut(shortcutText.text)
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
                height: 56

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

                Item {
                    id: titleRow
                    anchors.left: backButton.right
                    anchors.leftMargin: 12
                    anchors.right: parent.right
                    anchors.verticalCenter: backButton.verticalCenter
                    height: 48

                    Text {
                        id: settingTitle
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Settings"
                        color: foreground
                        font.pixelSize: fontSizeTitle
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Row {
                        id: statusRow
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8

                        StatusPill {
                            label: selectedProvider === "ollama" ? "LOCAL" : "CLOUD"
                            dotColor: selectedProvider === "ollama" ? accentPurple : accent
                        }

                        StatusPill {
                            label: selectedModel.length > 0 ? selectedModel : "NO MODEL"
                            dotColor: modelDetector.error !== "" ? root.error : accent
                            width: Math.min(190, implicitWidth)
                        }
                    }
                }
            }

            SettingsCard {
                title: "AI Provider"

                Row {
                    width: parent.width
                    spacing: 10

                    FieldLabel {
                        id: providerLabel
                        title: "Provider"
                    }

                    ThemedComboBox {
                        id: providerCombo
                        width: parent.width - providerLabel.width - 10
                        model: ["OpenAI", "Ollama"]
                        currentIndex: 0

                        onCurrentIndexChanged: {
                            if (!ready) {
                                return
                            }
                            // Suppress the text-input autosave while the
                            // provider's fields are being swapped in.
                            persistCurrentModel()
                            ready = false
                            selectedProvider = providerFromIndex(currentIndex)
                            applyProviderSettings(selectedProvider)
                            ready = true
                            saveConfig()
                            queueAutoDetectModels()
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 7
                    visible: true
                    height: visible ? implicitHeight : 0

                    Row {
                        width: parent.width
                        spacing: 10

                        FieldLabel {
                            id: serverLabel
                            title: "API Server"
                        }

                        TextField {
                            id: serverInput
                            width: parent.width - serverLabel.width - 10
                            height: 44
                            text: defaultOpenAiApiServer
                            color: foreground
                            placeholderText: defaultOpenAiApiServer
                            placeholderTextColor: subtleForeground
                            font.pixelSize: fontSizeNormal
                            leftPadding: 14
                            rightPadding: 14
                            verticalAlignment: TextInput.AlignVCenter
                            onTextChanged: {
                                if (ready) {
                                    saveConfigSoon()
                                    queueAutoDetectModels()
                                }
                            }

                            background: Rectangle {
                                color: serverInput.activeFocus ? "#152332" : fieldColor
                                radius: root.radius
                                border.width: 1
                                border.color: serverInput.activeFocus ? borderActive : (serverInput.hovered ? Qt.rgba(0.39, 0.96, 0.84, 0.55) : border)
                                Behavior on color { ColorAnimation { duration: 160 } }
                            }
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
                        spacing: 10

                        FieldLabel {
                            id: keyLabel
                            title: "API Key"
                        }

                        ScrollView {
                            id: keyScroll
                            width: parent.width - keyLabel.width - 10
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
                                topPadding: 10
                                bottomPadding: 10
                                leftPadding: 14
                                rightPadding: 14
                                onTextChanged: {
                                    if (ready) {
                                        saveConfigSoon()
                                        queueAutoDetectModels()
                                    }
                                }

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
            }

            SettingsCard {
                title: "Model Router"

                Row {
                    width: parent.width
                    spacing: 10

                    FieldLabel {
                        id: modelLabel
                        title: "Model"
                    }

                    Item {
                        id: modelRow
                        width: parent.width - modelLabel.width - 10
                        height: 44

                        TextField {
                            id: modelInput
                            anchors.fill: parent
                            text: selectedModel
                            color: foreground
                            placeholderText: modelDetector.availableModels.length > 0 ? "" : "Detect models first"
                            placeholderTextColor: subtleForeground
                            font.pixelSize: fontSizeNormal
                            leftPadding: 14
                            rightPadding: 100
                            onTextChanged: {
                                selectedModel = text.trim()
                                if (ready) {
                                    saveConfigSoon()
                                }
                            }

                            background: Rectangle {
                                color: modelInput.activeFocus || modelPopup.opened ? "#152332" : fieldColor
                                radius: root.radius
                                border.width: 1
                                border.color: modelPopup.opened ? borderActive : (modelInput.hovered || modelInput.activeFocus ? Qt.rgba(0.39, 0.96, 0.84, 0.55) : border)
                                Behavior on color { ColorAnimation { duration: 160 } }
                            }
                        }

                        Rectangle {
                            id: modelActionBtn
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 92
                            radius: root.radius
                            color: modelActionArea.containsMouse ? accentHover : accent
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.18)

                            Text {
                                anchors.centerIn: parent
                                text: modelDetector.isDetecting ? "Scanning"
                                      : modelPopup.opened ? "▴"
                                      : modelDetector.availableModels.length > 0 ? "Select ▾"
                                      : "Detect"
                                color: "#061014"
                                font.pixelSize: fontSizeNormal
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                id: modelActionArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (modelPopup.opened) {
                                        modelPopup.close()
                                    } else if (modelDetector.availableModels.length > 0) {
                                        modelPopup.open()
                                    } else if (!modelDetector.isDetecting) {
                                        openModelPopupAfterDetect = true
                                        detectModels()
                                    }
                                }
                            }
                        }
                    }
                }

                Row {
                    width: parent.width
                    spacing: 10

                    FieldLabel {
                        id: thinkingLabel
                        title: "Thinking"
                    }

                    ThemedComboBox {
                        id: thinkingCombo
                        width: parent.width - thinkingLabel.width - 10
                        model: ["Auto (detect by model)", "Qwen 3.8 / 3.6 / 3.5", "DeepSeek", "OpenAI o-series", "Ollama", "Manual (none)"]
                        currentIndex: thinkingIndex(setting.thinkingMode)

                        onActivated: {
                            if (ready) {
                                setting.thinkingMode = thinkingModeFromIndex(currentIndex)
                                saveConfig()
                            }
                        }
                    }
                }

                Text {
                    width: parent.width
                    leftPadding: thinkingLabel.width + 10
                    text: "Auto detects the model family from its name. If your model id is a relay alias (e.g. LocalModel on NewAPI), pick the family of the real model — e.g. Qwen 3.8 sends enable_thinking=false + reasoning_effort=low to skip xhigh thinking."
                    color: subtleForeground
                    font.pixelSize: 15
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }

                Popup {
                    id: modelPopup
                    x: modelRow.mapToItem(parent, 0, 0).x
                    y: modelRow.mapToItem(parent, 0, 0).y + modelRow.height + 6
                    width: modelRow.width
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

                Rectangle {
                    width: parent.width
                    height: modelDetectTip.visible ? 34 : 0
                    visible: modelDetectTip.text.length > 0
                    radius: 10
                    color: modelDetector.error !== "" ? Qt.rgba(1.0, 0.48, 0.56, 0.10) : Qt.rgba(0.39, 0.96, 0.84, 0.09)
                    border.width: 1
                    border.color: modelDetector.error !== "" ? Qt.rgba(1.0, 0.48, 0.56, 0.35) : Qt.rgba(0.39, 0.96, 0.84, 0.28)

                    Text {
                        id: modelDetectTip
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        text: modelDetector.error !== "" ? modelDetector.error
                                                                      : (modelDetector.availableModels.length > 0 ? (modelDetector.availableModels.length + " models detected · click Select to choose") : "")
                        color: modelDetector.error !== "" ? root.error : mutedForeground
                        font.pixelSize: fontSizeSmall
                        elide: Text.ElideRight
                    }
                }
            }

            SettingsCard {
                title: "Global Hotkey"

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
                            if (shortcutBeforeRecording.length > 0 && shortcutRegistrar !== null) {
                                shortcutRegistrar.setShortcut("")
                            }
                        } else if (!shortcutRecordingAccepted && shortcutBeforeRecording.length > 0 && shortcutRegistrar !== null) {
                            shortcutRegistrar.setShortcut(shortcutBeforeRecording)
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
                                font.pixelSize: 22
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
                            if (shortcutRegistrar !== null && shortcutRegistrar.setShortcut(shortcut) === false) {
                                if (shortcutBeforeRecording.length > 0) {
                                    shortcutRegistrar.setShortcut(shortcutBeforeRecording)
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

        }
    }

    ModelDiscoveryService {
        id: modelDetector
        onAvailableModelsChanged: {
            if (openModelPopupAfterDetect && availableModels.length > 0) {
                modelPopup.open()
                openModelPopupAfterDetect = false
            }
        }
        onIsDetectingChanged: {
            if (!isDetecting && availableModels.length === 0) {
                openModelPopupAfterDetect = false
            }
        }
    }

    Timer {
        id: saveDebounceTimer
        interval: 400
        repeat: false
        onTriggered: saveConfig()
    }

    Timer {
        id: autoDetectTimer
        interval: 700
        repeat: false
        onTriggered: {
            if (!isOllamaProvider && keyInput.text.trim().length < 10) {
                return
            }
            if (autoDetectProvider !== selectedProvider) {
                return
            }
            detectModels()
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
            font.pixelSize: 15
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
    }

    component FieldLabel: Rectangle {
        property string title: ""
        width: 110
        implicitWidth: 110
        height: 44
        color: "transparent"

        Text {
            id: labelText
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: mutedForeground
            text: title
            width: 92
            elide: Text.ElideRight
            font.pixelSize: fontSizeNormal
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
        }

        Rectangle {
            id: accentBar
            width: 4
            height: 12
            radius: 2
            color: accent
            x: 98
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}

import QtQuick
import QtQuick.Controls
import TransAI

Item {
    id: root
    signal backClicked

    property bool ready: false
    property string selectedProvider: "openai"
    property bool openModelPopupAfterDetect: false
    // Single source of truth for the model name: the input field itself.
    // (Previously a separate property mirrored the text, which forced manual
    // re-sync everywhere because the text binding broke on first user input.)
    property alias selectedModel: modelInput.text
    property string autoDetectServer: ""
    property string autoDetectKey: ""
    property string autoDetectProvider: ""
    property string shortcutBeforeRecording: ""
    property bool shortcutRecordingAccepted: false
    property QtObject shortcutRegistrar: null
    // Bound from main.qml (GlobalShortcutManager.hotkeysSupported). False on
    // Wayland/headless, where the recorder is replaced by an explanation.
    property bool hotkeysSupported: true
    // Last hotkey-registration failure, shown inline. main.qml also writes
    // here when registration fails outside a recording session.
    property string hotkeyError: ""
    readonly property bool isOllamaProvider: selectedProvider === "ollama"
    // Single source of truth for the thinking-mode list; the index order must
    // match the thinkingCombo model below.
    readonly property var thinkingModes: ["auto", "qwen", "deepseek", "openai", "ollama", "manual"]
    // Defaults live in ProviderDefaults (C++); Setting re-exposes them so the
    // UI never hardcodes URLs or model names in parallel.
    readonly property string defaultOllamaApiServer: setting.defaultOllamaApiServer
    readonly property string defaultOpenAiApiServer: setting.defaultOpenAiApiServer
    readonly property string defaultOpenAiModel: setting.defaultOpenAiModel

    // 主题颜色（统一来自 ThemeColors 单例；字号/圆角保留本页自己的尺度）
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
    readonly property color warning: ThemeColors.warningText
    readonly property color infoText: ThemeColors.infoText
    readonly property color fieldColorFocus: ThemeColors.fieldColorFocus
    readonly property color rowHighlightColor: ThemeColors.rowHighlightColor
    readonly property color cardColor: ThemeColors.cardColor
    readonly property int fontSizeLabel: 12
    readonly property int fontSizeSmall: 13
    readonly property int fontSizeNormal: 15
    readonly property int fontSizeLarge: 17
    readonly property int fontSizeTitle: 21
    readonly property real radius: 10
    readonly property real radiusLarge: 16
    readonly property int controlHeight: 40

    function providerFromIndex(index) {
        return index === 1 ? "ollama" : "openai"
    }

    function providerIndex(provider) {
        return provider === "ollama" ? 1 : 0
    }

    function thinkingModeFromIndex(index) {
        return root.thinkingModes[index] || "auto"
    }

    function thinkingIndex(mode) {
        const i = root.thinkingModes.indexOf(mode)
        return i >= 0 ? i : 0
    }

    // Server used for detection requests: the typed value, else the provider
    // default (the C++ side applies the same fallback to an empty string;
    // resolving it here additionally keeps the auto-detect dedup key stable).
    function effectiveServer() {
        const typed = serverInput.text.trim()
        if (typed.length > 0) {
            return typed
        }
        return isOllamaProvider ? defaultOllamaApiServer : defaultOpenAiApiServer
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
            selectedModel = setting.openaiModel.trim().length > 0 ? setting.openaiModel : defaultOpenAiModel
        }
        setting.applyProviderConfig(selectedProvider)
    }

    function reload() {
        ready = false
        setting.loadConfig()
        selectedProvider = setting.provider === "ollama" ? "ollama" : "openai"
        providerCombo.currentIndex = providerIndex(selectedProvider)
        // applyProviderSettings 覆盖 server/key/model 的回填，此处不再重复推导
        // （重复推导曾导致两处逻辑漂移的风险）。
        applyProviderSettings(selectedProvider)
        shortcutText.text = setting.shortCut

        ready = true
        Qt.callLater(function() {
            if (selectedProvider === "ollama" || setting.hasPlausibleApiKey(keyInput.text)) {
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
            setting.openaiModel = selectedModel.trim().length > 0 ? selectedModel.trim() : defaultOpenAiModel
        }
        setting.shortCut = shortcutText.text
        setting.applyProviderConfig(selectedProvider)
        setting.updateConfig()
        savedHint.opacity = 1
        savedTimer.restart()
    }

    // Debounced save for text inputs: writes at most once per pause in typing
    // instead of once per keystroke.
    function saveConfigSoon() {
        if (!ready) {
            return
        }
        saveDebounceTimer.restart()
    }

    // Writes pending debounced edits immediately. Must run BEFORE the
    // provider fields are swapped (provider switch, window close), otherwise
    // the pending edits of the outgoing provider would be dropped silently.
    function flushPendingSave() {
        saveDebounceTimer.stop()
        saveConfig()
    }

    function detectModels() {
        modelDetector.detectModels(effectiveServer(), keyInput.text, selectedProvider)
    }

    function queueAutoDetectModels() {
        if (!ready) {
            return
        }
        const server = effectiveServer()
        const key = isOllamaProvider ? "" : keyInput.text.trim()
        if (!isOllamaProvider && !setting.hasPlausibleApiKey(key)) {
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
        if ((event.key >= Qt.Key_A && event.key <= Qt.Key_Z)
            || (event.key >= Qt.Key_0 && event.key <= Qt.Key_9)) {
            return String.fromCharCode(event.key)
        }
        // Qt.Key_F1..Qt.Key_F12 are consecutive values.
        if (event.key >= Qt.Key_F1 && event.key <= Qt.Key_F12) {
            return "F" + (event.key - Qt.Key_F1 + 1)
        }
        return ""
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
        anchors.margins: 16
        contentHeight: contentColumn.height + 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {
            policy: flickable.contentHeight > flickable.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            width: 6
        }

        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 12

            Item {
                id: header
                width: parent.width
                height: 44

                RoundIconButton {
                    id: backButton
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 38
                    height: 38
                    label: "‹"
                    onClicked: backClicked()
                }

                Item {
                    id: titleRow
                    anchors.left: backButton.right
                    anchors.leftMargin: 12
                    anchors.right: parent.right
                    anchors.verticalCenter: backButton.verticalCenter
                    height: 40

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

                    Text {
                        id: savedHint
                        anchors.right: statusRow.left
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Saved ✓"
                        color: accent
                        font.pixelSize: fontSizeLabel
                        opacity: 0
                        Behavior on opacity { NumberAnimation { duration: 250 } }
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
                            label: selectedModel.trim().length > 0 ? selectedModel.trim() : "NO MODEL"
                            dotColor: modelDetector.error !== "" ? root.error : accent
                            width: Math.min(170, implicitWidth)
                        }
                    }
                }
            }

            SettingsCard {
                title: "AI Provider"
                subtitle: "Where translation requests are processed."

                Column {
                    width: parent.width
                    spacing: 6

                    FieldLabel { text: "PROVIDER" }

                    ThemedComboBox {
                        id: providerCombo
                        width: parent.width
                        model: ["OpenAI / Compatible API", "Ollama (Local)"]
                        currentIndex: 0

                        // onActivated 仅在用户交互时触发；reload() 的程序化赋值
                        // 直接调用 applyProviderSettings，无需 ready 门控。
                        onActivated: function(index) {
                            // Persist the outgoing provider's pending edits
                            // (including the model) while its fields are still
                            // in the inputs, then swap in the new provider's values.
                            flushPendingSave()
                            selectedProvider = providerFromIndex(index)
                            applyProviderSettings(selectedProvider)
                            saveConfig()
                            queueAutoDetectModels()
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 6

                    FieldLabel { text: "API SERVER" }

                    TextField {
                        id: serverInput
                        width: parent.width
                        height: controlHeight
                        text: defaultOpenAiApiServer
                        color: foreground
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
                            color: serverInput.activeFocus ? fieldColorFocus : fieldColor
                            radius: root.radius
                            border.width: 1
                            border.color: serverInput.activeFocus ? borderActive : (serverInput.hovered ? Qt.alpha(root.accent, 0.55) : border)
                            Behavior on color { ColorAnimation { duration: 160 } }
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 6
                    visible: !isOllamaProvider

                    FieldLabel { text: "API KEY" }

                    Item {
                        width: parent.width
                        height: controlHeight

                        TextField {
                            id: keyInput
                            anchors.fill: parent
                            echoMode: keyToggle.show ? TextInput.Normal : TextInput.Password
                            passwordCharacter: "•"
                            color: foreground
                            font.pixelSize: fontSizeNormal
                            leftPadding: 14
                            rightPadding: 54
                            verticalAlignment: TextInput.AlignVCenter
                            onTextChanged: {
                                if (ready) {
                                    saveConfigSoon()
                                    queueAutoDetectModels()
                                }
                            }

                            background: Rectangle {
                                color: keyInput.activeFocus ? fieldColorFocus : fieldColor
                                radius: root.radius
                                border.width: 1
                                border.color: keyInput.activeFocus ? borderActive : (keyInput.hovered ? Qt.alpha(root.accent, 0.55) : border)
                                Behavior on color { ColorAnimation { duration: 160 } }
                            }
                        }

                        Text {
                            id: keyToggle
                            property bool show: false
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            visible: keyInput.text.length > 0
                            text: show ? "Hide" : "Show"
                            color: keyToggleArea.containsMouse ? accentHover : mutedForeground
                            font.pixelSize: fontSizeSmall
                            font.weight: Font.DemiBold

                            MouseArea {
                                id: keyToggleArea
                                anchors.fill: parent
                                anchors.margins: -6
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: keyToggle.show = !keyToggle.show
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        readonly property string trimmedKey: keyInput.text.trim()
                        visible: trimmedKey.length === 0 || !setting.hasPlausibleApiKey(trimmedKey)
                        text: trimmedKey.length === 0
                              ? "Required — paste the API key for your provider."
                              : "This key doesn't look like a typical API key. Double-check it if requests fail."
                        color: trimmedKey.length === 0 ? root.error : root.warning
                        font.pixelSize: fontSizeLabel
                        wrapMode: Text.WordWrap
                    }
                }
            }

            SettingsCard {
                title: "Model"
                subtitle: "Which model answers, and how its reasoning output is handled."

                Column {
                    width: parent.width
                    spacing: 6

                    FieldLabel { text: "MODEL" }

                    Row {
                        width: parent.width
                        spacing: 8

                        Item {
                            id: modelRow
                            width: parent.width - detectButton.width - 8
                            height: controlHeight

                            TextField {
                                id: modelInput
                                anchors.fill: parent
                                // selectedModel 是此 text 属性的别名（唯一数据源）。
                                color: foreground
                                font.pixelSize: fontSizeNormal
                                leftPadding: 14
                                rightPadding: 34
                                onTextChanged: {
                                    if (ready) {
                                        saveConfigSoon()
                                    }
                                }

                                background: Rectangle {
                                    color: modelInput.activeFocus || modelPopup.opened ? fieldColorFocus : fieldColor
                                    radius: root.radius
                                    border.width: 1
                                    border.color: modelPopup.opened ? borderActive : (modelInput.hovered || modelInput.activeFocus ? Qt.alpha(root.accent, 0.55) : border)
                                    Behavior on color { ColorAnimation { duration: 160 } }
                                }

                                // 自绘占位提示：Material 风格的 placeholderText 会浮动到
                                // 控件上方，与本页自带的 FieldLabel 重复且视觉断裂。
                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 34
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: modelInput.text.length === 0 && !modelInput.activeFocus
                                    text: modelDetector.availableModels.length > 0
                                          ? "Select from the list, or type a name"
                                          : "Type a model name, or detect from the server"
                                    color: subtleForeground
                                    font.pixelSize: fontSizeNormal
                                    elide: Text.ElideRight
                                }
                            }

                            Text {
                                id: modelDropdownArrow
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                visible: modelDetector.availableModels.length > 0
                                text: modelPopup.opened ? "▴" : "▾"
                                color: modelPopup.opened || modelArrowArea.containsMouse ? accentHover : mutedForeground
                                font.pixelSize: fontSizeNormal

                                MouseArea {
                                    id: modelArrowArea
                                    anchors.fill: parent
                                    anchors.margins: -8
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: modelPopup.opened ? modelPopup.close() : modelPopup.open()
                                }
                            }
                        }

                        Rectangle {
                            id: detectButton
                            width: 92
                            height: controlHeight
                            radius: root.radius
                            color: detectArea.containsMouse && !modelDetector.isDetecting ? accentHover : accent
                            opacity: modelDetector.isDetecting ? 0.65 : 1.0
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.18)

                            Text {
                                anchors.centerIn: parent
                                text: modelDetector.isDetecting ? "Scanning"
                                      : modelDetector.availableModels.length > 0 ? "Refresh"
                                      : "Detect"
                                color: "#061014"
                                font.pixelSize: fontSizeSmall
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                id: detectArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: modelDetector.isDetecting ? Qt.ArrowCursor : Qt.PointingHandCursor
                                onClicked: {
                                    if (!modelDetector.isDetecting) {
                                        openModelPopupAfterDetect = true
                                        detectModels()
                                    }
                                }
                            }
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 6

                    FieldLabel { text: "THINKING" }

                    ThemedComboBox {
                        id: thinkingCombo
                        width: parent.width
                        model: ["Auto (recommended)", "Qwen3", "DeepSeek", "OpenAI o-series", "Ollama", "None (manual)"]
                        currentIndex: thinkingIndex(setting.thinkingMode)

                        onActivated: {
                            if (ready) {
                                setting.thinkingMode = thinkingModeFromIndex(currentIndex)
                                saveConfig()
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: "Auto picks the right settings from the model name. If you access models through a relay that renames them, choose the family of the real model."
                        color: subtleForeground
                        font.pixelSize: fontSizeLabel
                        wrapMode: Text.WordWrap
                        lineHeight: 1.25
                    }
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
                            height: 34
                            hoverEnabled: true

                            contentItem: Text {
                                text: modelData
                                color: selectedModel.trim() === modelData ? accent : foreground
                                font.pixelSize: fontSizeNormal
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                leftPadding: 12
                                rightPadding: 12
                            }

                            background: Rectangle {
                                color: hovered ? rowHighlightColor : panelColorAlt
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
                    height: modelDetectTip.visible ? modelDetectTip.implicitHeight + 18 : 0
                    visible: modelDetectTip.text.length > 0
                    radius: 10
                    color: modelDetector.error !== "" ? Qt.alpha(root.error, 0.10) : Qt.alpha(root.accent, 0.09)
                    border.width: 1
                    border.color: modelDetector.error !== "" ? Qt.alpha(root.error, 0.35) : Qt.alpha(root.accent, 0.28)
                    Behavior on height { NumberAnimation { duration: 140 } }

                    Text {
                        id: modelDetectTip
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        text: modelDetector.error !== "" ? modelDetector.error
                              : (modelDetector.availableModels.length > 0 ? (modelDetector.availableModels.length + " models detected — pick one from the dropdown.") : "")
                        color: modelDetector.error !== "" ? root.error : mutedForeground
                        font.pixelSize: fontSizeSmall
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                }
            }

            SettingsCard {
                title: "Global Hotkey"
                subtitle: "Translate the current selection from any app."

                Rectangle {
                    width: parent.width
                    visible: !root.hotkeysSupported
                    height: visible ? waylandTip.implicitHeight + 18 : 0
                    radius: 10
                    color: Qt.alpha(root.infoText, 0.08)
                    border.width: 1
                    border.color: Qt.alpha(root.infoText, 0.25)

                    Text {
                        id: waylandTip
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        text: "Global hotkeys aren't available on Wayland. Instead, bind a shortcut in your desktop settings to run: TransAI --translate-selection"
                        color: root.infoText
                        font.pixelSize: fontSizeLabel
                        wrapMode: Text.WordWrap
                        lineHeight: 1.25
                    }
                }

                Rectangle {
                    id: shortcutRect
                    width: parent.width
                    height: 52
                    color: activeFocus ? fieldColorFocus : fieldColor
                    radius: root.radius
                    border.width: 1
                    border.color: activeFocus ? borderActive : border
                    opacity: root.hotkeysSupported ? 1.0 : 0.5
                    focus: false

                    onActiveFocusChanged: {
                        if (activeFocus) {
                            shortcutBeforeRecording = shortcutText.text
                            shortcutRecordingAccepted = false
                            hotkeyError = ""
                            if (shortcutBeforeRecording.length > 0 && shortcutRegistrar !== null) {
                                shortcutRegistrar.setShortcut("")
                            }
                        } else if (!shortcutRecordingAccepted && shortcutBeforeRecording.length > 0 && shortcutRegistrar !== null) {
                            shortcutRegistrar.setShortcut(shortcutBeforeRecording)
                        }
                    }

                    Behavior on color { ColorAnimation { duration: 160 } }

                    // 快捷键的唯一数据源；显示由下方键帽 chips 负责，本体不可见。
                    Text {
                        id: shortcutText
                        visible: false
                        text: ""
                        onTextChanged: saveConfigSoon()
                    }

                    Row {
                        id: chipRow
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.right: parent.right
                        anchors.rightMargin: shortcutRect.activeFocus ? 92 : (clearButton.visible ? 40 : 12)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        clip: true

                        Repeater {
                            model: !shortcutRect.activeFocus && shortcutText.text.length > 0 ? shortcutText.text.split("+") : []

                            delegate: Rectangle {
                                height: 30
                                width: chipText.implicitWidth + 20
                                radius: 8
                                color: fieldColorFocus
                                border.width: 1
                                border.color: Qt.alpha(root.accent, 0.35)

                                Text {
                                    id: chipText
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: foreground
                                    font.pixelSize: fontSizeSmall
                                    font.weight: Font.DemiBold
                                }
                            }
                        }

                        Text {
                            visible: shortcutText.text.length === 0 && !shortcutRect.activeFocus
                            text: root.hotkeysSupported ? "Click to record a shortcut" : "Unavailable on this platform"
                            color: subtleForeground
                            font.pixelSize: fontSizeSmall
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            visible: shortcutRect.activeFocus
                            text: "Press keys…"
                            color: accent
                            font.pixelSize: fontSizeSmall
                            font.weight: Font.DemiBold
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        visible: shortcutRect.activeFocus
                        text: "Esc to cancel"
                        color: subtleForeground
                        font.pixelSize: fontSizeLabel
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
                                hotkeyError = "Couldn't register '" + shortcut + "'. It may be reserved by the system or another app."
                                if (shortcutBeforeRecording.length > 0) {
                                    shortcutRegistrar.setShortcut(shortcutBeforeRecording)
                                }
                                shortcutRect.focus = false
                                event.accepted = true
                                return
                            }
                            shortcutRecordingAccepted = true
                            hotkeyError = ""
                            shortcutText.text = shortcut
                            shortcutRect.focus = false
                        }
                        event.accepted = true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: root.hotkeysSupported ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (root.hotkeysSupported) {
                                shortcutRect.forceActiveFocus()
                            }
                        }
                    }

                    Text {
                        id: clearButton
                        anchors.right: parent.right
                        anchors.rightMargin: 13
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.hotkeysSupported && !shortcutRect.activeFocus && shortcutText.text.length > 0
                        text: "✕"
                        color: clearArea.containsMouse ? root.error : mutedForeground
                        font.pixelSize: fontSizeNormal

                        MouseArea {
                            id: clearArea
                            anchors.fill: parent
                            anchors.margins: -8
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (shortcutRegistrar !== null) {
                                    shortcutRegistrar.setShortcut("")
                                }
                                hotkeyError = ""
                                // onTextChanged 触发 saveConfigSoon，无需手动保存
                                shortcutText.text = ""
                            }
                        }
                    }
                }

                Text {
                    width: parent.width
                    visible: root.hotkeysSupported && root.hotkeyError.length === 0
                    text: "Works system-wide: press it in any app to translate the currently selected text."
                    color: subtleForeground
                    font.pixelSize: fontSizeLabel
                    wrapMode: Text.WordWrap
                    lineHeight: 1.25
                }

                Text {
                    width: parent.width
                    visible: root.hotkeysSupported && root.hotkeyError.length > 0
                    text: root.hotkeyError
                    color: root.error
                    font.pixelSize: fontSizeLabel
                    wrapMode: Text.WordWrap
                    lineHeight: 1.25
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
        id: savedTimer
        interval: 1400
        repeat: false
        onTriggered: savedHint.opacity = 0
    }

    Timer {
        id: autoDetectTimer
        interval: 700
        repeat: false
        onTriggered: {
            if (!isOllamaProvider && !setting.hasPlausibleApiKey(keyInput.text.trim())) {
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
        height: cardColumn.height + 32
        radius: root.radiusLarge
        color: root.cardColor
        border.width: 1
        border.color: Qt.alpha(root.accent, 0.14)

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
            anchors.margins: 16
            spacing: 12

            Row {
                width: parent.width
                height: Math.max(24, titleColumn.implicitHeight)
                spacing: 10

                Rectangle {
                    width: 3
                    height: Math.max(18, titleColumn.implicitHeight)
                    radius: 1.5
                    anchors.verticalCenter: parent.verticalCenter
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: accentHover }
                        GradientStop { position: 1.0; color: accentPurple }
                    }
                }

                Column {
                    id: titleColumn
                    width: parent.width - 13
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3

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
                        visible: subtitle.length > 0
                        text: subtitle
                        color: subtleForeground
                        font.pixelSize: fontSizeLabel
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Column {
                id: bodyColumn
                width: parent.width
                spacing: 12
            }
        }
    }

    component RoundIconButton: Button {
        id: control
        property string label: ""
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
            font.pixelSize: 24
            font.weight: Font.Light
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component StatusPill: Rectangle {
        property string label: ""
        property color dotColor: accent
        // 宽度留 5px 余量：Text implicitWidth 与可用宽度完全相等时会触发 elide
        implicitWidth: pillText.implicitWidth + 32
        width: implicitWidth
        height: 22
        radius: 11
        color: Qt.rgba(1, 1, 1, 0.055)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.08)

        Rectangle {
            width: 6
            height: 6
            radius: 3
            anchors.left: parent.left
            anchors.leftMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            color: dotColor
        }

        Text {
            id: pillText
            anchors.left: parent.left
            anchors.leftMargin: 19
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: label
            color: mutedForeground
            font.pixelSize: fontSizeLabel
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
    }

    component FieldLabel: Text {
        color: mutedForeground
        font.pixelSize: fontSizeLabel
        font.weight: Font.DemiBold
        font.letterSpacing: 1.0
    }
}

import QtQuick
import QtQuick.Controls
import TransAI

Item {
    id: root
    signal backClicked

    property bool ready: false
    property string selectedProvider: "openai"
    property bool openModelPopupAfterDetect: false
    // 下拉过滤器：仅在弹窗展开期间由输入更新。不变式：弹窗关闭时恒为 ""
    // （onClosed 与每次打开前都会重置），因此点击打开总是展示完整列表。
    property string modelFilter: ""
    // modelPopup 最近一次关闭的时间戳（ms）。弹窗展开时点击输入框/箭头，按下会
    // 先通过 CloseOnPressOutside 关闭弹窗；该门控阻止同一次点击的抬起再把它打开，
    // 使点击表现为开关切换（无论弹窗遮罩是吃掉还是透传这次按下都成立）。
    property real modelPopupClosedAt: 0
    // Single source of truth for the model name: the input field itself.
    // (Previously a separate property mirrored the text, which forced manual
    // re-sync everywhere because the text binding broke on first user input.)
    property alias selectedModel: modelInput.text
    // 下拉列表数据源：已探测模型按 modelFilter 做大小写不敏感的子串过滤
    property var filteredModels: {
        const all = modelDetector.availableModels
        const needle = modelFilter.trim().toLowerCase()
        if (needle.length === 0) {
            return all
        }
        const out = []
        for (var i = 0; i < all.length; ++i) {
            if (all[i].toLowerCase().indexOf(needle) !== -1) {
                out.push(all[i])
            }
        }
        return out
    }
    property string autoDetectServer: ""
    property string autoDetectKey: ""
    property string autoDetectProvider: ""
    property string shortcutBeforeRecording: ""
    property bool shortcutRecordingAccepted: false
    property QtObject shortcutRegistrar: null
    // Same contract as shortcutRegistrar, but for the screenshot action
    // (main.qml wires it to the "translateScreenshot" action id).
    property QtObject screenshotShortcutRegistrar: null
    // Screenshot translation card state (vision model override + advanced).
    property string selectedVisionModel: ""
    property bool advancedExpanded: false
    // Bound from main.qml (GlobalShortcutManager.hotkeysSupported). False on
    // Wayland/headless, where the recorder is replaced by an explanation.
    property bool hotkeysSupported: true
    // Last hotkey-registration failure, shown inline. main.qml also writes
    // here when registration fails outside a recording session.
    property string hotkeyError: ""
    readonly property bool isOllamaProvider: selectedProvider === "ollama"
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
    readonly property int fontSizeLabel: 13
    readonly property int fontSizeSmall: 14
    readonly property int fontSizeNormal: 15
    readonly property int fontSizeLarge: 17
    readonly property int fontSizeTitle: 21
    readonly property real radius: 10
    readonly property real radiusLarge: 16
    readonly property int controlHeight: 40
    // 控件列的起始 x：所有行内控件从这一列对齐到右缘，形成稳定的两栏节奏
    readonly property real fieldColumnX: 120

    function providerFromIndex(index) {
        return index === 1 ? "ollama" : "openai"
    }

    function providerIndex(provider) {
        return provider === "ollama" ? 1 : 0
    }

    // Display-language combo mapping; the index order must match the
    // languageCombo model below (native names stay untranslated on purpose).
    function displayLanguageFromIndex(index) {
        return index === 1 ? "zh-CN" : "en"
    }

    function displayLanguageIndex(language) {
        return language === "zh-CN" ? 1 : 0
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
        languageCombo.currentIndex = displayLanguageIndex(setting.displayLanguage)

        // Screenshot translation card backfill.
        selectedVisionModel = setting.screenshotVisionModel
        const choices = visionModelChoices()
        visionModelCombo.currentIndex = Math.max(0, choices.indexOf(
            selectedVisionModel.trim().length > 0 ? selectedVisionModel.trim() : choices[0]))
        screenshotShortcutField.shortcut = setting.screenshotShortCut
        maxWidthInput.text = String(setting.screenshotMaxImageWidth)
        jpegQualityInput.text = String(setting.screenshotJpegQuality)

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
        setting.displayLanguage = displayLanguageFromIndex(languageCombo.currentIndex)
        // Screenshot translation. The mode key stays at its loaded/default
        // value ("auto"); only the vision model override, hotkey and advanced
        // image options are user-facing for now.
        setting.screenshotVisionModel = selectedVisionModel.trim()
        setting.screenshotShortCut = screenshotShortcutField.shortcut
        if (maxWidthInput.acceptableInput) {
            setting.screenshotMaxImageWidth = parseInt(maxWidthInput.text)
        }
        if (jpegQualityInput.acceptableInput) {
            setting.screenshotJpegQuality = parseInt(jpegQualityInput.text)
        }
        setting.applyProviderConfig(selectedProvider)
        setting.updateConfig()
        savedHint.opacity = 1
        savedTimer.restart()
    }

    // Vision-model combo entries: "follow current" first, then the stored
    // override (when absent from the detected list) and the detected models.
    function visionModelChoices() {
        const choices = [I18n.t("Follow current model")]
        const stored = selectedVisionModel.trim()
        if (stored.length > 0 && modelDetector.availableModels.indexOf(stored) === -1) {
            choices.push(stored)
        }
        return choices.concat(modelDetector.availableModels)
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

    // 统一的下拉打开入口：先重置过滤器，保证点击打开时总是展示完整列表
    // （按过滤器不变式，弹窗正常关闭后此处本就为空，属于防御性重置）。
    function openModelPopup() {
        modelFilter = ""
        modelPopup.open()
    }

    // 点击模型输入框：列表就绪则直接展开；为空则触发一次探测，由
    // openModelPopupAfterDetect 在结果到达后自动展开。探测进行中、或
    // OpenAI key 明显不可用时不动作（页面已有对应的错误提示）。
    function handleModelFieldTap() {
        if (modelDetector.availableModels.length > 0) {
            openModelPopup()
        } else if (!modelDetector.isDetecting
                   && (isOllamaProvider || setting.hasPlausibleApiKey(keyInput.text.trim()))) {
            openModelPopupAfterDetect = true
            detectModels()
        }
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
            screenshotShortcutField.focus = false
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
            spacing: 10

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
                        text: I18n.t("Settings")
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
                        text: I18n.t("Saved ✓")
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
                            label: selectedProvider === "ollama" ? I18n.t("LOCAL") : I18n.t("CLOUD")
                            dotColor: selectedProvider === "ollama" ? accentPurple : accent
                        }

                        StatusPill {
                            label: selectedModel.trim().length > 0 ? selectedModel.trim() : I18n.t("NO MODEL")
                            dotColor: modelDetector.error !== "" ? root.error : accent
                            width: Math.min(170, implicitWidth)
                        }
                    }
                }
            }

            SettingsCard {
                title: I18n.t("Language")

                headerData: ThemedComboBox {
                    id: languageCombo
                    width: parent.width
                    // Language names always stay in their native language.
                    model: ["English", "简体中文"]
                    currentIndex: displayLanguageIndex(setting.displayLanguage)

                    // Switching updates setting.displayLanguage; the I18n
                    // singleton follows it and re-evaluates every bound
                    // string live. saveConfig persists the change.
                    onActivated: function(index) {
                        saveConfig()
                    }
                }
            }

            SettingsCard {
                title: I18n.t("AI Provider")

                SettingsRow {
                    label: I18n.t("PROVIDER")

                    ThemedComboBox {
                        id: providerCombo
                        width: parent.width
                        model: [I18n.t("OpenAI / Compatible API"), I18n.t("Ollama (Local)")]
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

                SettingsRow {
                    label: I18n.t("API SERVER")

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

                SettingsRow {
                    label: I18n.t("API KEY")
                    visible: !isOllamaProvider

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
                            text: show ? I18n.t("Hide") : I18n.t("Show")
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
                }

                Text {
                    width: parent.width
                    // 缩进到控件列：提示从属于密钥输入框，与其左缘对齐，
                    // 保持卡片的两栏节奏不被顶格文字打断。
                    leftPadding: root.fieldColumnX
                    readonly property string trimmedKey: keyInput.text.trim()
                    visible: !isOllamaProvider && (trimmedKey.length === 0 || !setting.hasPlausibleApiKey(trimmedKey))
                    text: trimmedKey.length === 0
                          ? I18n.t("Required — paste the API key for your provider.")
                          : I18n.t("This key doesn't look like a typical API key. Double-check it if requests fail.")
                    color: trimmedKey.length === 0 ? root.error : root.warning
                    font.pixelSize: fontSizeLabel
                    wrapMode: Text.WordWrap
                }
            }

            SettingsCard {
                title: I18n.t("Model")

                headerData: Row {
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
                                    // 下拉展开时输入即过滤（自动补全）；activeFocus 门控
                                    // 避免程序化回填（reload / 切换 provider / 选中项）改动过滤器。
                                    if (modelPopup.opened && activeFocus) {
                                        root.modelFilter = text
                                    }
                                }

                                // Enter 直接采纳过滤结果：优先完全匹配，否则取第一条。
                                Keys.onPressed: (event) => {
                                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                        && modelPopup.opened && root.filteredModels.length > 0) {
                                        const exact = root.filteredModels.indexOf(text.trim())
                                        selectedModel = exact >= 0 ? root.filteredModels[exact] : root.filteredModels[0]
                                        modelPopup.close()
                                        saveConfig()
                                        event.accepted = true
                                    }
                                }

                                // 可编辑下拉框交互：点击输入框即展开下拉；列表为空时改为触发
                                // 一次探测并在结果到达后自动展开。弹窗展开时同一次点击的按下会
                                // 先被 CloseOnPressOutside 用来关闭弹窗，时间戳门控保证抬起不会
                                // 再打开，因此点击表现为开关切换。
                                TapHandler {
                                    onTapped: {
                                        if (Date.now() - root.modelPopupClosedAt > 250) {
                                            root.handleModelFieldTap()
                                        }
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
                                // 控件上方，与本页行内标签的排版重复且视觉断裂。
                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 34
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: modelInput.text.length === 0 && !modelInput.activeFocus
                                    text: modelDetector.availableModels.length > 0
                                          ? I18n.t("Select from the list, or type a name")
                                          : I18n.t("Type a model name, or detect from the server")
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
                                    onClicked: {
                                        // 与输入框点击共用时间戳门控：这次按下若已通过
                                        // CloseOnPressOutside 关闭了弹窗，不再重新打开。
                                        if (modelPopup.opened) {
                                            modelPopup.close()
                                        } else if (Date.now() - root.modelPopupClosedAt > 250) {
                                            root.openModelPopup()
                                        }
                                    }
                                }
                            }

                            // 下拉列表以输入框为锚（Popup 的 parent 即 modelRow）：x 恒为 0、
                            // y 恒相对输入框，布局变化或页面滚动时始终与输入框左缘对齐，
                            // 不会像旧的 mapToItem 绑定那样只求值一次后失效。
                            Popup {
                                id: modelPopup
                                // 展开方向与高度上限在打开瞬间（onAboutToShow）按窗口内
                                // 上/下方可用空间计算：下方放不下完整列表且上方更宽敞时
                                // 向上展开；高度收敛到可用空间内，避免被窗口边缘裁剪。
                                property bool openUpward: false
                                property real listMaxHeight: 260

                                x: 0
                                y: openUpward ? -implicitHeight - 6 : modelRow.height + 6
                                width: modelRow.width
                                implicitHeight: root.filteredModels.length === 0 ? 40 : Math.min(modelList.contentHeight + 2, listMaxHeight)
                                padding: 1
                                // 不抢键盘焦点：弹窗展开时焦点留在输入框，打字继续过滤
                                focus: false
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                                onAboutToShow: {
                                    const rowY = modelRow.mapToItem(root, 0, 0).y
                                    const below = root.height - rowY - modelRow.height - 12
                                    const above = rowY - 12
                                    const desired = Math.min(modelList.contentHeight + 2, 260)
                                    openUpward = below < desired && above > below
                                    listMaxHeight = Math.max(102, Math.min(260, openUpward ? above : below))
                                    // 长列表中把当前选中的模型滚动到可见位置；
                                    // 打开前过滤器已重置，filteredModels 即完整列表
                                    const current = root.filteredModels.indexOf(selectedModel.trim())
                                    if (current >= 0) {
                                        modelList.positionViewAtIndex(current, ListView.Center)
                                    }
                                }

                                // 关闭时恢复过滤器不变式，并记录时间戳供点击门控使用
                                onClosed: {
                                    root.modelFilter = ""
                                    root.modelPopupClosedAt = Date.now()
                                }

                                contentItem: Item {
                                    ListView {
                                        id: modelList
                                        anchors.fill: parent
                                        clip: true
                                        model: root.filteredModels
                                        ScrollIndicator.vertical: ScrollIndicator { }

                                        delegate: ItemDelegate {
                                            width: modelList.width
                                            height: 34
                                            hoverEnabled: true
                                            // 选项不抢焦点：点选后键盘焦点留在输入框，可继续输入
                                            focusPolicy: Qt.NoFocus

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
                                                // 先关弹窗再赋值：赋值会触发 modelInput.onTextChanged，
                                                // 若弹窗仍开着会更新 modelFilter → ListView model 变化 →
                                                // 本委托被销毁，后续语句抛 ReferenceError。
                                                const picked = modelData
                                                modelPopup.close()
                                                selectedModel = picked
                                                saveConfig()
                                            }
                                        }
                                    }

                                    // 过滤无结果时的空态提示（此时手动输入仍然有效）
                                    Text {
                                        anchors.centerIn: parent
                                        visible: root.filteredModels.length === 0
                                        text: I18n.t("No matching models")
                                        color: subtleForeground
                                        font.pixelSize: fontSizeSmall
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
                                text: modelDetector.isDetecting ? I18n.t("Scanning")
                                      : modelDetector.availableModels.length > 0 ? I18n.t("Refresh")
                                      : I18n.t("Detect")
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
                              : (modelDetector.availableModels.length > 0 ? I18n.t("%1 models detected — pick one from the dropdown.", modelDetector.availableModels.length) : "")
                        color: modelDetector.error !== "" ? root.error : mutedForeground
                        font.pixelSize: fontSizeSmall
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                }
            }

            SettingsCard {
                title: I18n.t("Screenshot Translation")

                SettingsRow {
                    label: I18n.t("VISION MODEL")

                    ThemedComboBox {
                        id: visionModelCombo
                        width: parent.width
                        model: root.visionModelChoices()
                        onActivated: function(index) {
                            root.selectedVisionModel = index === 0 ? "" : model[index]
                            saveConfig()
                        }
                    }
                }

                Text {
                    width: parent.width
                    leftPadding: root.fieldColumnX
                    readonly property string effectiveVisionModel: root.selectedVisionModel.trim().length > 0
                        ? root.selectedVisionModel.trim() : root.selectedModel.trim()
                    visible: effectiveVisionModel.length > 0 && !setting.isLikelyVisionModel(effectiveVisionModel)
                    text: I18n.t("This model may not support image input. Pick a vision-capable model above if screenshot translation fails.")
                    color: root.warning
                    font.pixelSize: fontSizeLabel
                    wrapMode: Text.WordWrap
                    lineHeight: 1.25
                }

                Rectangle {
                    width: parent.width
                    visible: !root.hotkeysSupported
                    height: visible ? screenshotWaylandTip.implicitHeight + 18 : 0
                    radius: 10
                    color: Qt.alpha(root.infoText, 0.08)
                    border.width: 1
                    border.color: Qt.alpha(root.infoText, 0.25)

                    Text {
                        id: screenshotWaylandTip
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        text: I18n.t("On Wayland, bind a shortcut in your desktop settings to run: TransAI --translate-screenshot")
                        color: root.infoText
                        font.pixelSize: fontSizeLabel
                        wrapMode: Text.WordWrap
                        lineHeight: 1.25
                    }
                }

                SettingsRow {
                    label: I18n.t("HOTKEY")

                    ShortcutRecorder {
                        id: screenshotShortcutField
                        width: parent.width
                        registrar: root.screenshotShortcutRegistrar
                        onEdited: saveConfigSoon()
                    }
                }

                Text {
                    width: parent.width
                    visible: screenshotShortcutField.errorText.length > 0
                    text: screenshotShortcutField.errorText
                    color: root.error
                    font.pixelSize: fontSizeLabel
                    wrapMode: Text.WordWrap
                    lineHeight: 1.25
                }

                Text {
                    text: (root.advancedExpanded ? "▾ " : "▸ ") + I18n.t("Advanced")
                    color: advancedToggleArea.containsMouse ? root.accentHover : root.mutedForeground
                    font.pixelSize: fontSizeSmall
                    font.weight: Font.DemiBold

                    MouseArea {
                        id: advancedToggleArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.advancedExpanded = !root.advancedExpanded
                    }
                }

                Column {
                    width: parent.width
                    spacing: 10
                    visible: root.advancedExpanded

                    SettingsRow {
                        label: I18n.t("MAX IMAGE WIDTH")

                        TextField {
                            id: maxWidthInput
                            width: 140
                            height: controlHeight
                            color: foreground
                            font.pixelSize: fontSizeNormal
                            leftPadding: 14
                            rightPadding: 14
                            verticalAlignment: TextInput.AlignVCenter
                            validator: IntValidator { bottom: 512; top: 8192 }
                            onTextChanged: saveConfigSoon()

                            background: Rectangle {
                                color: maxWidthInput.activeFocus ? fieldColorFocus : fieldColor
                                radius: root.radius
                                border.width: 1
                                border.color: maxWidthInput.activeFocus ? borderActive : (maxWidthInput.hovered ? Qt.alpha(root.accent, 0.55) : border)
                                Behavior on color { ColorAnimation { duration: 160 } }
                            }
                        }
                    }

                    SettingsRow {
                        label: I18n.t("JPEG QUALITY")

                        TextField {
                            id: jpegQualityInput
                            width: 140
                            height: controlHeight
                            color: foreground
                            font.pixelSize: fontSizeNormal
                            leftPadding: 14
                            rightPadding: 14
                            verticalAlignment: TextInput.AlignVCenter
                            validator: IntValidator { bottom: 10; top: 100 }
                            onTextChanged: saveConfigSoon()

                            background: Rectangle {
                                color: jpegQualityInput.activeFocus ? fieldColorFocus : fieldColor
                                radius: root.radius
                                border.width: 1
                                border.color: jpegQualityInput.activeFocus ? borderActive : (jpegQualityInput.hovered ? Qt.alpha(root.accent, 0.55) : border)
                                Behavior on color { ColorAnimation { duration: 160 } }
                            }
                        }
                    }
                }
            }

            SettingsCard {
                title: I18n.t("Global Hotkey")

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
                        text: I18n.t("Global hotkeys aren't available on Wayland. Instead, bind a shortcut in your desktop settings to run: TransAI --translate-selection")
                        color: root.infoText
                        font.pixelSize: fontSizeLabel
                        wrapMode: Text.WordWrap
                        lineHeight: 1.25
                    }
                }

                headerData: Rectangle {
                    id: shortcutRect
                    width: parent.width
                    height: 44
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
                            text: root.hotkeysSupported ? I18n.t("Click to record a shortcut") : I18n.t("Unavailable on this platform")
                            color: subtleForeground
                            font.pixelSize: fontSizeSmall
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            visible: shortcutRect.activeFocus
                            text: I18n.t("Press keys…")
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
                        text: I18n.t("Esc to cancel")
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
                                hotkeyError = I18n.t("Couldn't register '%1'. It may be reserved by the system or another app.", shortcut)
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
                openModelPopup()
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
        id: card
        width: parent ? parent.width : 0
        height: cardColumn.height + 28
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
        // 标题行右侧的内联控件槽（选项框/输入框等），与标题同行
        property alias headerData: headerArea.data
        property string title: ""

        Column {
            id: cardColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 14
            spacing: 10

            Item {
                id: cardHeader
                width: parent.width
                height: Math.max(18, titleText.implicitHeight, headerArea.childrenRect.height)

                Rectangle {
                    width: 3
                    height: 18
                    radius: 1.5
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: accentHover }
                        GradientStop { position: 1.0; color: accentPurple }
                    }
                }

                Text {
                    id: titleText
                    anchors.left: parent.left
                    anchors.leftMargin: 13
                    anchors.verticalCenter: parent.verticalCenter
                    text: card.title
                    color: foreground
                    font.pixelSize: fontSizeLarge
                    font.weight: Font.DemiBold
                }

                Item {
                    id: headerArea
                    anchors.left: parent.left
                    anchors.leftMargin: Math.max(root.fieldColumnX, titleText.implicitWidth + 27)
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: childrenRect.height
                }
            }

            Column {
                id: bodyColumn
                width: parent.width
                spacing: 10
                height: childrenRect.height
            }
        }
    }

    // 左侧标签 + 右侧控件的单行布局；控件列从 fieldColumnX 对齐到右缘
    component SettingsRow: Item {
        id: settingsRow
        property string label: ""
        default property alias contentData: contentArea.data
        width: parent ? parent.width : 0
        height: Math.max(rowLabel.implicitHeight, contentArea.childrenRect.height)

        Text {
            id: rowLabel
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: settingsRow.label
            color: foreground
            font.pixelSize: fontSizeLarge
            font.weight: Font.DemiBold
            font.letterSpacing: 0.6
        }

        Item {
            id: contentArea
            anchors.left: parent.left
            anchors.leftMargin: Math.max(root.fieldColumnX, rowLabel.implicitWidth + 16)
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: childrenRect.height
        }
    }

    // Self-contained shortcut recorder (focus/record/cancel/restore cycle
    // mirrors the Global Hotkey card's recorder). `shortcut` is the single
    // data source; `registrar` applies the OS-level registration; `edited()`
    // fires after an accepted recording so the parent can persist.
    component ShortcutRecorder: Rectangle {
        id: recorder
        property string shortcut: ""
        property QtObject registrar: null
        property string errorText: ""
        property string beforeRecording: ""
        property bool recordingAccepted: false
        signal edited()

        height: 44
        color: activeFocus ? fieldColorFocus : fieldColor
        radius: root.radius
        border.width: 1
        border.color: activeFocus ? borderActive : border
        opacity: root.hotkeysSupported ? 1.0 : 0.5

        onActiveFocusChanged: {
            if (activeFocus) {
                beforeRecording = shortcut
                recordingAccepted = false
                errorText = ""
                if (beforeRecording.length > 0 && registrar !== null) {
                    registrar.setShortcut("")
                }
            } else if (!recordingAccepted && beforeRecording.length > 0 && registrar !== null) {
                registrar.setShortcut(beforeRecording)
            }
        }

        Behavior on color { ColorAnimation { duration: 160 } }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: recorder.activeFocus ? 92 : (recorderClear.visible ? 40 : 12)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            clip: true

            Repeater {
                model: !recorder.activeFocus && recorder.shortcut.length > 0 ? recorder.shortcut.split("+") : []

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
                visible: recorder.shortcut.length === 0 && !recorder.activeFocus
                text: root.hotkeysSupported ? I18n.t("Click to record a shortcut") : I18n.t("Unavailable on this platform")
                color: subtleForeground
                font.pixelSize: fontSizeSmall
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                visible: recorder.activeFocus
                text: I18n.t("Press keys…")
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
            visible: recorder.activeFocus
            text: I18n.t("Esc to cancel")
            color: subtleForeground
            font.pixelSize: fontSizeLabel
        }

        Keys.onPressed: (event) => {
            if (!recorder.activeFocus) {
                return
            }

            if (event.key === Qt.Key_Escape) {
                recorder.focus = false
                event.accepted = true
                return
            }

            var shortcut = shortcutFromEvent(event)
            if (shortcut.length > 0) {
                if (recorder.registrar !== null && recorder.registrar.setShortcut(shortcut) === false) {
                    recorder.errorText = I18n.t("Couldn't register '%1'. It may be reserved by the system or another app.", shortcut)
                    if (recorder.beforeRecording.length > 0) {
                        recorder.registrar.setShortcut(recorder.beforeRecording)
                    }
                    recorder.focus = false
                    event.accepted = true
                    return
                }
                recorder.recordingAccepted = true
                recorder.errorText = ""
                recorder.shortcut = shortcut
                recorder.edited()
                recorder.focus = false
            }
            event.accepted = true
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: root.hotkeysSupported ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: {
                if (root.hotkeysSupported) {
                    recorder.forceActiveFocus()
                }
            }
        }

        Text {
            id: recorderClear
            anchors.right: parent.right
            anchors.rightMargin: 13
            anchors.verticalCenter: parent.verticalCenter
            visible: root.hotkeysSupported && !recorder.activeFocus && recorder.shortcut.length > 0
            text: "✕"
            color: recorderClearArea.containsMouse ? root.error : mutedForeground
            font.pixelSize: fontSizeNormal

            MouseArea {
                id: recorderClearArea
                anchors.fill: parent
                anchors.margins: -8
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (recorder.registrar !== null) {
                        recorder.registrar.setShortcut("")
                    }
                    recorder.errorText = ""
                    recorder.shortcut = ""
                    recorder.edited()
                }
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
}

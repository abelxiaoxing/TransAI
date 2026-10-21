import QtQuick
import QtQuick.Controls
import TransAI
import "."
import "settings"

// Settings page skeleton: header (back/title/saved hint/status pills) plus
// the five section cards. Card UIs live in qml/settings/; this file keeps
// the cross-card orchestration: provider switching, config load/save,
// auto model detection and the debounced-save machinery.
//
// Public API (used by main.qml): reload(), flushPendingSave(), hotkeyError /
// screenshotHotkeyError (aliases for the per-card registration errors),
// shortcutRegistrar, hotkeysSupported.
Item {
    id: root
    signal backClicked

    property bool ready: false
    property string selectedProvider: "openai"
    // Single source of truth for the model name lives in the Model card's
    // input field; this alias keeps the header pill and saveConfig simple.
    property alias selectedModel: modelSection.modelText
    // main.qml writes here when a hotkey registration fails outside a
    // recording session; the Hotkey card displays it inline.
    property alias hotkeyError: hotkeySection.hotkeyError
    // Same contract for the screenshot hotkey (action id "translateScreenshot");
    // both recorders live in the Global Hotkey card.
    property alias screenshotHotkeyError: hotkeySection.screenshotHotkeyError
    property QtObject shortcutRegistrar: null
    // Bound from main.qml (GlobalShortcutManager.hotkeysSupported). False on
    // Wayland/headless, where the recorder is replaced by an explanation.
    property bool hotkeysSupported: true
    property string autoDetectServer: ""
    property string autoDetectKey: ""
    property string autoDetectProvider: ""
    readonly property bool isOllamaProvider: selectedProvider === "ollama"
    // Defaults live in ProviderDefaults (C++); Setting re-exposes them so the
    // UI never hardcodes URLs or model names in parallel.
    readonly property string defaultOllamaApiServer: setting.defaultOllamaApiServer
    readonly property string defaultOpenAiApiServer: setting.defaultOpenAiApiServer
    readonly property string defaultOpenAiModel: setting.defaultOpenAiModel

    // 主题颜色（统一来自 ThemeColors 单例；字号保留本页自己的尺度）
    readonly property color foreground: ThemeColors.foreground
    readonly property color mutedForeground: ThemeColors.mutedForeground
    readonly property color accent: ThemeColors.accent
    readonly property color accentPurple: ThemeColors.accentPurple
    readonly property color error: ThemeColors.error
    readonly property int fontSizeLabel: 13
    readonly property int fontSizeTitle: 21

    function providerFromIndex(index) {
        return index === 1 ? "ollama" : "openai"
    }

    function providerIndex(provider) {
        return provider === "ollama" ? 1 : 0
    }

    // Display-language combo mapping; the index order must match the
    // LanguageSection model (native names stay untranslated on purpose).
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
        const typed = providerSection.serverText.trim()
        if (typed.length > 0) {
            return typed
        }
        return isOllamaProvider ? defaultOllamaApiServer : defaultOpenAiApiServer
    }

    function applyProviderSettings(provider) {
        var isOllama = provider === "ollama"
        selectedProvider = isOllama ? "ollama" : "openai"
        modelSection.openPopupAfterDetect = false
        modelSection.closePopup()

        if (isOllama) {
            providerSection.serverText = setting.ollamaApiServer.trim().length > 0 ? setting.ollamaApiServer : defaultOllamaApiServer
            selectedModel = setting.ollamaModel.trim().length > 0 ? setting.ollamaModel : ""
        } else {
            providerSection.serverText = setting.openaiApiServer.trim().length > 0 ? setting.openaiApiServer : defaultOpenAiApiServer
            providerSection.keyText = setting.openaiApiKey
            selectedModel = setting.openaiModel.trim().length > 0 ? setting.openaiModel : defaultOpenAiModel
        }
        setting.applyProviderConfig(selectedProvider)
    }

    function reload() {
        ready = false
        setting.loadConfig()
        selectedProvider = setting.provider === "ollama" ? "ollama" : "openai"
        providerSection.providerIndex = providerIndex(selectedProvider)
        // applyProviderSettings 覆盖 server/key/model 的回填，此处不再重复推导
        // （重复推导曾导致两处逻辑漂移的风险）。
        applyProviderSettings(selectedProvider)
        hotkeySection.shortcut = setting.shortCut
        hotkeySection.screenshotShortcut = setting.screenshotShortCut
        languageSection.languageIndex = displayLanguageIndex(setting.displayLanguage)

        // Screenshot translation card backfill.
        screenshotSection.restoreMode(setting.screenshotTranslateMode)
        screenshotSection.restoreVisionModel(setting.screenshotVisionModel)
        screenshotSection.restoreAdvanced(setting.screenshotMaxImageWidth, setting.screenshotJpegQuality)

        ready = true
        Qt.callLater(function() {
            if (selectedProvider === "ollama" || setting.hasPlausibleApiKey(providerSection.keyText)) {
                detectModels()
            }
        })
    }

    function saveConfig() {
        if (!ready) {
            return
        }
        if (isOllamaProvider) {
            setting.ollamaApiServer = providerSection.serverText.trim().length > 0 ? providerSection.serverText.trim() : defaultOllamaApiServer
            setting.ollamaModel = selectedModel.trim()
        } else {
            const server = providerSection.serverText.trim().length > 0 ? providerSection.serverText.trim() : defaultOpenAiApiServer
            setting.openaiApiServer = server
            setting.openaiApiKey = providerSection.keyText
            setting.openaiModel = selectedModel.trim().length > 0 ? selectedModel.trim() : defaultOpenAiModel
        }
        setting.shortCut = hotkeySection.shortcut
        setting.screenshotShortCut = hotkeySection.screenshotShortcut
        setting.displayLanguage = displayLanguageFromIndex(languageSection.languageIndex)
        // Screenshot translation. The routing mode (auto/vision/ocr), the
        // vision model override and the advanced image options.
        setting.screenshotTranslateMode = screenshotSection.translateMode
        setting.screenshotVisionModel = screenshotSection.selectedVisionModel.trim()
        if (screenshotSection.maxImageWidth() > 0) {
            setting.screenshotMaxImageWidth = screenshotSection.maxImageWidth()
        }
        if (screenshotSection.jpegQuality() > 0) {
            setting.screenshotJpegQuality = screenshotSection.jpegQuality()
        }
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
        modelSection.detect(effectiveServer(), providerSection.keyText, selectedProvider)
    }

    function queueAutoDetectModels() {
        if (!ready) {
            return
        }
        const server = effectiveServer()
        const key = isOllamaProvider ? "" : providerSection.keyText.trim()
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

    AppBackground {
        accentCircleTopMargin: 90
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            hotkeySection.releaseFocus()
            hotkeySection.reapplyShortcut()
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
                    // 让位给窗口级置顶按钮（main.qml 的左上角 chrome）
                    anchors.leftMargin: 34
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
                            dotColor: modelSection.detector.error !== "" ? root.error : accent
                            width: Math.min(170, implicitWidth)
                        }
                    }
                }
            }

            LanguageSection {
                id: languageSection
                onLanguageActivated: saveConfig()
            }

            ProviderSection {
                id: providerSection
                ollama: root.isOllamaProvider
                defaultServerText: root.defaultOpenAiApiServer
                onProviderActivated: function(index) {
                    // Persist the outgoing provider's pending edits
                    // (including the model) while its fields are still
                    // in the inputs, then swap in the new provider's values.
                    flushPendingSave()
                    selectedProvider = providerFromIndex(index)
                    applyProviderSettings(selectedProvider)
                    saveConfig()
                    queueAutoDetectModels()
                }
                onServerEdited: {
                    if (ready) {
                        saveConfigSoon()
                        queueAutoDetectModels()
                    }
                }
                onKeyEdited: {
                    if (ready) {
                        saveConfigSoon()
                        queueAutoDetectModels()
                    }
                }
            }

            ModelSection {
                id: modelSection
                detectAllowed: root.isOllamaProvider || setting.hasPlausibleApiKey(providerSection.keyText.trim())
                onEdited: {
                    if (ready) {
                        saveConfigSoon()
                    }
                }
                onSaveRequested: saveConfig()
                onDetectRequested: detectModels()
            }

            HotkeySection {
                id: hotkeySection
                hotkeysSupported: root.hotkeysSupported
                shortcutRegistrar: root.shortcutRegistrar
                onEdited: saveConfigSoon()
            }

            ScreenshotSection {
                id: screenshotSection
                detector: modelSection.detector
                currentModel: root.selectedModel
                onEdited: saveConfigSoon()
                onSaveRequested: saveConfig()
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
            if (!isOllamaProvider && !setting.hasPlausibleApiKey(providerSection.keyText.trim())) {
                return
            }
            if (autoDetectProvider !== selectedProvider) {
                return
            }
            detectModels()
        }
    }
}

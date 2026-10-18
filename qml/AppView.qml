import QtQuick
import QtQuick.Controls
import TransAI
import "."

Item {
    id:root
    property alias inputText: inputArea.text
    property bool inputHasFocus: inputArea.textedit.activeFocus
    property bool resultHasFocus: result.textedit.activeFocus
    property bool stopped: false
    property bool selectionUnavailable: false
    // Raw (English) app-generated error currently shown in the result panel,
    // kept so a live UI-language switch can re-translate it (the display
    // points assign localized text imperatively, which breaks bindings).
    property string lastRawError: ""
    property bool errorDisplayed: false
    // Screenshot (vision) session state. Exactly one session owns the result
    // panel at a time: starting either path aborts the other service first.
    property bool screenshotSession: false
    // OCR leg of a screenshot session is in flight (tesseract/Vision running).
    property bool ocrBusy: false
    // Set when the capture itself failed before any request was sent.
    property bool screenshotFailed: false
    // Request snapshot: captured when a translation starts, so the history
    // entry and any follow-up action describe what was actually sent — even if
    // the user edits the input field while the answer is still streaming.
    property string requestSourceText: ""
    property string requestTargetLanguage: ""
    // Pending streamed text for the throttled result-panel refresh. A TextEdit
    // relayout (plus scroll-to-bottom) per SSE chunk is measurably wasteful on
    // long answers, so chunks are coalesced into ~20 updates/second.
    property string pendingResultText: ""
    // Key of the request whose answer was served from the history store
    // (see reuseCachedTranslation). Non-empty ⇒ the result panel shows a
    // cached translation and no request is in flight; pressing Translate
    // again on the same request forces a real translation.
    property string cacheHitKey: ""

    // Derived view state: exactly one write-path per flag (translation
    // lifecycle signals below), everything else is a binding so the final
    // status never depends on signal-delivery order.
    readonly property bool statusBusy: translationService.isRequesting || visionService.isRequesting || root.ocrBusy
    readonly property string activeError: screenshotSession ? visionService.responseError : translationService.responseError
    readonly property string statusText: {
        if (selectionUnavailable) return I18n.t("Selection unavailable")
        if (root.ocrBusy) return I18n.t("Recognizing text...")
        if (statusBusy) return I18n.t("Translating...")
        if (screenshotSession && screenshotFailed) return I18n.t("Error")
        if (activeError !== "") return I18n.t("Error")
        if (stopped) return I18n.t("Stopped")
        if (root.cacheHitKey !== "") return I18n.t("From history")
        return I18n.t("Ready")
    }
    signal settingClicked;
    // 显式关闭请求（✕ 按钮）；main.qml 收到后播放隐藏动画。Wayland 等平台
    // 无法感知“点击窗口外”，必须提供一个明确的鼠标关闭入口。
    signal closeRequested();

    // 主题颜色（统一来自 ThemeColors 单例）
    readonly property color accent: ThemeColors.accent
    readonly property color accentHover: ThemeColors.accentHover
    readonly property color border: ThemeColors.border
    readonly property color borderActive: ThemeColors.borderActive
    readonly property color panelColor: ThemeColors.panelColor
    readonly property color panelColorAlt: ThemeColors.panelColorAlt
    readonly property color error: ThemeColors.error
    readonly property color borderIdle: ThemeColors.borderIdle
    readonly property color accentForeground: ThemeColors.accentForeground
    readonly property color accentForegroundDisabled: ThemeColors.accentForegroundDisabled
    readonly property color infoBg: ThemeColors.infoBg
    readonly property color warningBg: ThemeColors.warningBg
    readonly property color infoText: ThemeColors.infoText
    readonly property color mutedForeground: ThemeColors.mutedForeground
    readonly property color warningText: ThemeColors.warningText
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeNormal: 16
    readonly property int fontSizeLarge: 18
    readonly property real radius: 8
    function startTrans(){
        if (inputArea.text.length === 0) {
            return
        }
        const sourceText = inputArea.text
        const targetLanguage = effectiveTargetLang(sourceText)
        // Re-translating the request whose cached answer is on screen counts as
        // an explicit "do it for real" (a repeated key press fixes a stale or
        // unsatisfying cached result instead of showing it forever).
        const allowCache = root.cacheHitKey !== root.translationCacheKey(sourceText, targetLanguage)
        // The text session takes over the result panel.
        visionService.abort()
        ocrEngine.cancel()
        root.ocrBusy = false
        root.screenshotSession = false
        root.screenshotFailed = false
        root.cacheHitKey = ""
        // Snapshot the request before sending: the input field stays editable
        // while the answer streams, and the history entry must describe what
        // was actually translated.
        root.requestSourceText = sourceText
        root.requestTargetLanguage = targetLanguage
        if (allowCache && root.reuseCachedTranslation(sourceText, targetLanguage)) {
            return
        }
        translationService.translate(root.requestSourceText,
                                     root.requestTargetLanguage,
                                     setting.provider,
                                     setting.apiServer,
                                     setting.apiKey,
                                     setting.model)
    }

    // One identical request = same source text and target language (whitespace
    // around the source is ignored). The history entry is never rewritten by a
    // cache hit, so the newest saved answer to that sentence stays served.
    function translationCacheKey(sourceText, targetLanguage) {
        return targetLanguage + "\n" + sourceText.trim()
    }

    // Serves an identical earlier translation from the history store instead of
    // hitting the provider again. Returns true when the result panel was
    // filled. The cache *is* the history list (nothing extra is stored).
    function reuseCachedTranslation(sourceText, targetLanguage) {
        const cached = translationHistory.findCachedResult(sourceText, targetLanguage)
        if (cached === "") {
            return false
        }
        root.requestSourceText = sourceText
        root.requestTargetLanguage = targetLanguage
        root.beginSession()
        root.pendingResultText = cached
        root.flushResultText()
        root.cacheHitKey = root.translationCacheKey(sourceText, targetLanguage)
        return true
    }

    // Screenshot routing from setting.screenshotTranslateMode: explicit
    // "vision"/"ocr" win; "auto" picks vision when the effective model looks
    // vision-capable, OCR otherwise (or vision as the last resort, which then
    // shows the vision-unsupported guidance error).
    function effectiveScreenshotMode() {
        const mode = setting.screenshotTranslateMode
        if (mode === "vision" || mode === "ocr") {
            return mode
        }
        if (setting.isLikelyVisionModel(setting.effectiveScreenshotVisionModel())) {
            return "vision"
        }
        return ocrEngine.available ? "ocr" : "vision"
    }

    // Screenshot translation entry point. The image comes from
    // ScreenshotCaptureService; the vision path uses the screenshot vision
    // model override, the OCR path recognizes text first and then runs it
    // through the normal text pipeline.
    function startScreenshotTranslation(image) {
        translationService.abort()
        visionService.abort()
        ocrEngine.cancel()
        root.screenshotSession = true
        root.screenshotFailed = false
        // A new image session never shows a cached answer.
        root.cacheHitKey = ""
        // Pure image sessions have no text source; the OCR branch fills this
        // in with the recognized text once it arrives.
        root.requestSourceText = ""
        if (effectiveScreenshotMode() === "ocr") {
            if (!ocrEngine.available) {
                // The backend knows why (missing models, no tesseract, ...);
                // its reason is a fixed English sentence that localizeError()
                // translates when the table has an entry for it.
                const reason = ocrEngine.unavailableReason
                showScreenshotError(reason.length > 0
                                    ? reason
                                    : "OCR is not available on this platform")
                return
            }
            root.ocrBusy = true
            root.requestTargetLanguage = effectiveScreenshotTargetLang()
            ocrEngine.recognize(image)
            return
        }
        root.requestTargetLanguage = effectiveScreenshotTargetLang()
        visionService.translateImage(image,
                                     root.requestTargetLanguage,
                                     setting.provider,
                                     setting.apiServer,
                                     setting.apiKey,
                                     setting.effectiveScreenshotVisionModel(),
                                     setting.screenshotMaxImageWidth,
                                     setting.screenshotJpegQuality)
    }

    // Capture failed before any request could be sent (portal unavailable,
    // platform unsupported, ...). Distinct from a translation failure.
    function showScreenshotError(message) {
        visionService.abort()
        resultFlushTimer.stop()
        root.pendingResultText = ""
        root.screenshotSession = true
        root.screenshotFailed = true
        root.lastRawError = message
        root.errorDisplayed = true
        result.text = localizeError(message)
    }

    function showSelectionError(message) {
        translationService.abort()
        resultFlushTimer.stop()
        root.pendingResultText = ""
        selectionUnavailable = true
        root.lastRawError = message
        root.errorDisplayed = true
        result.text = localizeError(message)
    }

    // True when the text contains CJK ideographs / Japanese kana / Korean
    // hangul — the heuristic behind the "Auto" target-language direction.
    function containsCjk(text) {
        return /[\u3400-\u9fff\uf900-\ufaff\u3040-\u30ff\uac00-\ud7af]/.test(text)
    }

    // Resolves the effective target language for text translation. With the
    // "Auto" selector entry (index 0), the direction follows the source
    // script: CJK input → English, everything else → 简体中文.
    function effectiveTargetLang(sourceText) {
        if (langSelector.currentIndex !== 0) {
            return langSelector.currentText
        }
        return containsCjk(sourceText) ? "English" : "简体中文"
    }

    // Auto direction for screenshots: the source text is not known up front,
    // so follow the UI language (Chinese UI → 简体中文, otherwise English).
    function effectiveScreenshotTargetLang() {
        if (langSelector.currentIndex !== 0) {
            return langSelector.currentText
        }
        return I18n.isChinese ? "简体中文" : "English"
    }

    // Localizes the fixed, app-generated error strings while leaving dynamic
    // server payloads untouched. Composite "<guidance>\n\n<raw api error>"
    // messages (e.g. the vision-unsupported hint) translate only the head
    // when it has a table entry; the raw tail is kept for troubleshooting.
    function localizeError(message) {
        const sep = message.indexOf("\n\n")
        if (sep > 0) {
            const head = message.substring(0, sep)
            const translatedHead = I18n.t(head)
            if (translatedHead !== head) {
                return translatedHead + message.substring(sep)
            }
        }
        return I18n.t(message)
    }

    // Result-panel lifecycle shared by both translation services. Each service
    // only forwards its signals here, so the panel behaves identically for
    // text, vision and OCR sessions.
    function beginSession() {
        resultFlushTimer.stop()
        pendingResultText = ""
        result.text = ""
        stopped = false
        selectionUnavailable = false
        errorDisplayed = false
        lastRawError = ""
        cacheHitKey = ""
    }

    // Streamed content: coalesce into the throttled flush (ignored while an
    // error is being shown).
    function updateResultText(data, error) {
        if (error !== "") {
            return
        }
        pendingResultText = data
        if (!resultFlushTimer.running) {
            resultFlushTimer.start()
        }
    }

    function flushResultText() {
        resultFlushTimer.stop()
        if (result.text !== pendingResultText) {
            result.text = pendingResultText
        }
    }

    // App/server error: shown immediately (no throttling), then kept as the raw
    // English string so a live language switch can re-render it.
    function showServiceError(message) {
        if (message === "") {
            return
        }
        resultFlushTimer.stop()
        lastRawError = message
        errorDisplayed = true
        result.text = localizeError(message)
    }

    // Terminal success: flush the tail, then record the history entry from the
    // request snapshot (not from the possibly-edited input field).
    function finishSession(isScreenshot, resultText) {
        flushResultText()
        translationHistory.addEntry(isScreenshot ? "screenshot" : "text",
                                    root.requestSourceText,
                                    resultText,
                                    root.requestTargetLanguage)
    }

    // Restores a history entry into the panels.
    function restoreHistoryEntry(entry) {
        const source = entry.source || ""
        if (source.length > 0) {
            inputArea.text = source
        }
        resultFlushTimer.stop()
        root.pendingResultText = ""
        result.text = entry.result
        root.screenshotSession = entry.type === "screenshot"
        root.screenshotFailed = false
        root.errorDisplayed = false
        root.lastRawError = ""
        root.stopped = false
        root.selectionUnavailable = false
        root.cacheHitKey = ""
        historyPopup.close()
    }

    // Throttled result-panel refresh (see pendingResultText).
    Timer {
        id: resultFlushTimer
        interval: 50
        repeat: false
        onTriggered: {
            if (root.pendingResultText !== result.text) {
                result.text = root.pendingResultText
            }
        }
    }

    AppBackground {
        // Defaults match the previous hand-rolled layout.
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
            text: I18n.t("Quick Translate")
            color: root.accent
            anchors.left: parent.left
            // 让位给窗口级置顶按钮（main.qml 的左上角 chrome）
            anchors.leftMargin: 32
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
            color: Qt.alpha(root.infoText, 0.12)
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
            accessibleName: I18n.t("Settings")
            onClicked: {
                settingClicked();
            }
        }

        Text {
            id: closeButton
            z: 1
            width: 26
            height: 26
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 16
            text: "✕"
            color: closeArea.containsMouse ? root.accentHover : root.mutedForeground
            font.pixelSize: root.fontSizeNormal
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            Accessible.role: Accessible.Button
            Accessible.name: I18n.t("Close")
            Accessible.onPressAction: root.closeRequested()
            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.closeRequested()
            }
        }

        // History button: opens the recent-translations popup. Styled like
        // the ✕ close button (muted → accent on hover).
        Text {
            id: historyButton
            z: 1
            height: 26
            anchors.right: settingBtn.left
            anchors.bottom: parent.bottom
            anchors.rightMargin: 12
            verticalAlignment: Text.AlignVCenter
            text: I18n.t("History")
            color: historyArea.containsMouse ? root.accentHover : root.mutedForeground
            font.pixelSize: root.fontSizeSmall
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Button
            Accessible.name: I18n.t("Translation history")
            Accessible.onPressAction: historyPopup.open()
            MouseArea {
                id: historyArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: historyPopup.open()
            }
        }

        Rectangle {
            id: statusPill
            anchors.top: parent.top
            // 右上角依次是 ✕ 关闭、设置和历史按钮，pill 让位到历史按钮左侧，
            // 避免与 closeButton 交叠（曾出现 ✕ 直接压在状态文字上）
            anchors.right: historyButton.left
            anchors.rightMargin: 8
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

    // Recent translations (newest first). Selecting an entry restores its
    // source/result into the panels; the popup itself only reports the pick.
    MainHistoryPopup {
        id: historyPopup
        z: 10
        onEntryActivated: function(entry) { root.restoreHistoryEntry(entry) }
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
            border.color: root.inputHasFocus ? root.borderActive : root.borderIdle
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

    // 置顶按钮已上移到 main.qml 的窗口级 chrome（翻译页与设置页共用，恒定
    // 停在窗口左上角），这里不再维护页面内的副本。
    Text{
        id:indictor
        text: I18n.t("Translated")
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

    // Copies the whole result text without requiring a selection. Shows a
    // brief "Copied ✓" confirmation (same feedback pattern as "Saved ✓").
    Text {
        id: copyButton
        z: 2
        visible: result.text.length > 0
        anchors.right: inputItem.right
        anchors.rightMargin: 10
        anchors.verticalCenter: indictor.verticalCenter
        text: copyFeedbackTimer.running ? I18n.t("Copied ✓") : I18n.t("Copy")
        color: copyFeedbackTimer.running ? root.accent
                                         : (copyArea.containsMouse ? root.accentHover : root.mutedForeground)
        font.pixelSize: root.fontSizeSmall
        font.weight: Font.DemiBold
        Accessible.role: Accessible.Button
        Accessible.name: I18n.t("Copy result")
        Accessible.onPressAction: {
            clipboard.setText(result.text)
            copyFeedbackTimer.restart()
        }
        MouseArea {
            id: copyArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                clipboard.setText(result.text)
                copyFeedbackTimer.restart()
            }
        }
        Timer {
            id: copyFeedbackTimer
            interval: 1500
        }
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
        text: Qt.platform.os === "macos" ? I18n.t("Translate ⌘Enter") : I18n.t("Translate (Ctrl+Enter)")
        font.capitalization: Font.MixedCase
        enabled:inputArea.text.length > 0
        visible: !root.statusBusy
        onClicked: {
            root.startTrans()
        }
        height:50
        background: Rectangle {
            color: transBtn.enabled
                ? (transBtn.pressed ? Qt.darker(root.accent, 1.35) : (transBtn.hovered ? root.accentHover : root.accent))
                : Qt.alpha(root.accent, 0.35)
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
            color: transBtn.enabled ? root.accentForeground : root.accentForegroundDisabled
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
        running: root.statusBusy
        visible:root.statusBusy
        width:transBtn.height - 10
        height:width
    }
    Button{
        id:stopBtn
        visible: root.statusBusy
        z:2
        anchors.bottom: parent.bottom
        anchors.right:parent.right
        anchors.margins:10
        text:I18n.t("Stop")
        onClicked: {
            translationService.abort()
            visionService.abort()
            ocrEngine.cancel()
            root.ocrBusy = false
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

    ThemedComboBox {
        id: langSelector
        z: 2
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 10
        // The first entry is the localized "Auto" (direction follows the
        // source script / UI language); the rest are native language names,
        // deliberately not localized.
        model: [I18n.t("Auto"), "简体中文", "繁体中文", "English", "Japanese", "German", "Korean", "Español", "français"]
        // Restore the persisted target language. "Auto" is stored as the
        // never-localized sentinel "__auto__" so a UI-language switch keeps
        // the selection; an empty/unknown stored value falls back to it.
        Component.onCompleted: {
            if (setting.targetLanguage === "__auto__") {
                currentIndex = 0
                return
            }
            const idx = model.indexOf(setting.targetLanguage)
            if (idx > 0) {
                currentIndex = idx
            }
        }
        // onActivated only fires for user picks, so restoring above does not
        // trigger a redundant config write.
        onActivated: {
            setting.targetLanguage = currentIndex === 0 ? "__auto__" : currentText
            setting.updateConfig()
        }
    }

    // Both services write into the same result panel; starting either session
    // aborts the other first, so they never race. Their signal forwarding is
    // identical on purpose — the shared logic lives in beginSession /
    // updateResultText / showServiceError / finishSession above.
    TranslationService {
        id: translationService
        onTranslationStarted: root.beginSession()
        onResponseDataChanged: root.updateResultText(responseData, responseError)
        onResponseErrorChanged: root.showServiceError(responseError)
        onTranslationFinished: function(resultText) { root.finishSession(root.screenshotSession, resultText) }
        onTranslationStopped: {
            // Keep the partial result visible, but distinguish it from Ready.
            root.flushResultText()
            root.stopped = true
        }
    }

    // Screenshot (multimodal) translation.
    VisionTranslationService {
        id: visionService
        onTranslationStarted: root.beginSession()
        onResponseDataChanged: root.updateResultText(responseData, responseError)
        onResponseErrorChanged: root.showServiceError(responseError)
        onTranslationFinished: function(resultText) { root.finishSession(true, resultText) }
        onTranslationStopped: {
            root.flushResultText()
            root.stopped = true
        }
    }

    // OCR leg completion: the recognized text continues through the normal
    // text pipeline. Superseded sessions (a text
    // translation started meanwhile) drop late OCR results.
    Connections {
        target: ocrEngine
        function onRecognized(text) {
            root.ocrBusy = false
            if (!root.screenshotSession) {
                return
            }
            const trimmed = text.trim()
            if (trimmed.length === 0) {
                root.showScreenshotError("No text recognized in the screenshot")
                return
            }
            inputArea.text = trimmed
            visionService.abort()
            root.cacheHitKey = ""
            // The recognized text may already have a saved translation (from an
            // earlier screenshot or from typing it): reuse it instead of paying
            // for another request.
            if (root.reuseCachedTranslation(trimmed, root.effectiveScreenshotTargetLang())) {
                return
            }
            // The recognized text is this session's source: it goes to the
            // input panel and into the history entry.
            root.requestSourceText = trimmed
            root.requestTargetLanguage = root.effectiveScreenshotTargetLang()
            translationService.translate(trimmed, root.requestTargetLanguage,
                                         setting.provider, setting.apiServer,
                                         setting.apiKey, setting.model)
        }
        function onRecognitionFailed(message) {
            root.ocrBusy = false
            if (!root.screenshotSession) {
                return
            }
            root.showScreenshotError(message)
        }
    }

    // Errors are displayed through imperative assignments (bindings would be
    // broken by streaming writes), so a live UI-language switch re-renders
    // the currently shown error explicitly.
    Connections {
        target: I18n
        function onLanguageChanged() {
            if (root.errorDisplayed && !root.statusBusy && root.lastRawError !== "") {
                result.text = root.localizeError(root.lastRawError)
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import TransAI
import "."

Item {
    id:root
    property bool pinned: false
    property alias inputText: inputArea.text
    property bool inputHasFocus: inputArea.textedit.activeFocus
    property bool resultHasFocus: result.textedit.activeFocus
    property bool stopped: false
    property bool selectionUnavailable: false
    // Screenshot (vision) session state. Exactly one session owns the result
    // panel at a time: starting either path aborts the other service first.
    property bool screenshotSession: false
    // Set when the capture itself failed before any request was sent.
    property bool screenshotFailed: false

    // Derived view state: exactly one write-path per flag (translation
    // lifecycle signals below), everything else is a binding so the final
    // status never depends on signal-delivery order.
    readonly property bool statusBusy: translationService.isRequesting || visionService.isRequesting
    readonly property string activeError: screenshotSession ? visionService.responseError : translationService.responseError
    readonly property string statusText: {
        if (selectionUnavailable) return I18n.t("Selection unavailable")
        if (statusBusy) return I18n.t("Translating...")
        if (screenshotSession && screenshotFailed) return I18n.t("Error")
        if (activeError !== "") return I18n.t("Error")
        if (stopped) return I18n.t("Stopped")
        return I18n.t("Ready")
    }
    signal settingClicked;
    // 显式关闭请求（✕ 按钮）；main.qml 收到后播放隐藏动画。Wayland 等平台
    // 无法感知“点击窗口外”，必须提供一个明确的鼠标关闭入口。
    signal closeRequested();
    // Emitted when the user (or startup) changes the pinned state; main.qml
    // applies the Qt.WindowStaysOnTopHint flag — AppView never touches the
    // window directly.
    signal pinRequested(bool pinned)

    // Single source of truth for "stays on top". The pin icon derives from
    // it and main.qml's focus-loss logic reads it.
    function setPinned(value) {
        pinned = value
        pinRequested(value)
    }

    // 主题颜色（统一来自 ThemeColors 单例）
    readonly property color accent: ThemeColors.accent
    readonly property color accentHover: ThemeColors.accentHover
    readonly property color border: ThemeColors.border
    readonly property color borderActive: ThemeColors.borderActive
    readonly property color panelColor: ThemeColors.panelColor
    readonly property color panelColorAlt: ThemeColors.panelColorAlt
    readonly property color error: ThemeColors.error
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
        // The text session takes over the result panel.
        visionService.abort()
        root.screenshotSession = false
        root.screenshotFailed = false
        translationService.translateFromMap({
            "text": inputArea.text,
            "targetLanguage": langSelector.currentText,
            "mode": getMode(),
            "provider": setting.provider,
            "apiServer": setting.apiServer,
            "apiKey": setting.apiKey,
            "model": setting.model
        })
    }

    // Screenshot translation entry point (multimodal). The image comes from
    // ScreenshotCaptureService; options mirror the text request but use the
    // screenshot vision model override ("follow current model" when empty).
    function startScreenshotTranslation(image) {
        translationService.abort()
        root.screenshotSession = true
        root.screenshotFailed = false
        visionService.translateImage(image, {
            "targetLanguage": langSelector.currentText,
            "provider": setting.provider,
            "apiServer": setting.apiServer,
            "apiKey": setting.apiKey,
            "model": setting.effectiveScreenshotVisionModel(),
            "maxImageWidth": setting.screenshotMaxImageWidth,
            "jpegQuality": setting.screenshotJpegQuality
        })
    }

    // Capture failed before any request could be sent (portal unavailable,
    // platform unsupported, ...). Distinct from a translation failure.
    function showScreenshotError(message) {
        visionService.abort()
        root.screenshotSession = true
        root.screenshotFailed = true
        result.text = message
    }

    function showSelectionError(message) {
        translationService.abort()
        selectionUnavailable = true
        result.text = message
    }

    // Mode selector state is setting.translationMode (persisted); indexes
    // align with translationModeFromInt() (0=Translate, 1=ExplainWord,
    // 2=CorrectGrammar).
    function getMode(){
        return setting.translationMode
    }

    // Per-mode translate-button label (mode selector drives the binding).
    function actionLabel() {
        const mac = Qt.platform.os === "macos"
        if (setting.translationMode === 1) return mac ? I18n.t("Explain ⌘Enter") : I18n.t("Explain (Ctrl+Enter)")
        if (setting.translationMode === 2) return mac ? I18n.t("Correct ⌘Enter") : I18n.t("Correct (Ctrl+Enter)")
        return mac ? I18n.t("Translate ⌘Enter") : I18n.t("Translate (Ctrl+Enter)")
    }

    // Per-mode result-panel indicator.
    function resultLabel() {
        if (root.screenshotSession) return I18n.t("Translated")
        if (setting.translationMode === 1) return I18n.t("Explanation")
        if (setting.translationMode === 2) return I18n.t("Corrected")
        return I18n.t("Translated")
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
            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.closeRequested()
            }
        }

        Rectangle {
            id: statusPill
            anchors.top: parent.top
            // 右上角依次是 ✕ 关闭和设置按钮，pill 让位到设置按钮左侧，
            // 避免与 closeButton 交叠（曾出现 ✕ 直接压在状态文字上）
            anchors.right: settingBtn.left
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

    // Text-translation mode selector: segmented control above the input.
    // Selection state is a binding on setting.translationMode, so config
    // restore and UI-language switches need no imperative handling. The
    // screenshot vision path ignores the mode (PromptBuilder is not used).
    Row {
        id: modeBar
        z: 2
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.margins: 10
        spacing: 6
        property var modes: [I18n.t("Translate"), I18n.t("Explain Word"), I18n.t("Correct Grammar")]

        Repeater {
            model: modeBar.modes
            delegate: Rectangle {
                width: segText.implicitWidth + 20
                height: 26
                radius: 13
                color: index === setting.translationMode ? root.accent
                     : (segArea.containsMouse ? Qt.alpha(root.accent, 0.18) : root.panelColor)
                border.width: 1
                border.color: index === setting.translationMode ? root.accent : root.border
                Text {
                    id: segText
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: root.fontSizeSmall
                    font.weight: Font.DemiBold
                    color: index === setting.translationMode ? "#0F1721" : root.mutedForeground
                }
                MouseArea {
                    id: segArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        setting.translationMode = index
                        setting.updateConfig()
                    }
                }
            }
        }
    }

    Item{
        z:2
        id:inputItem
        anchors.margins: 10
        anchors.top: modeBar.bottom
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

    // 置顶按钮：唯一状态源是 root.pinned，图标随其派生，不再单独维护 state
    Image {
        id: pinButton
        width: 22
        height: 22
        anchors.top: inputItem.bottom
        anchors.topMargin: 10
        anchors.right: inputItem.right
        source: root.pinned ? "qrc:///res/thumbtack_yes.png" : "qrc:///res/thumbtack_no.png"
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.setPinned(!root.pinned)
        }
    }

    Text{
        id:indictor
        text: root.resultLabel()
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
        text: root.actionLabel()
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
        text:I18n.t("stop")
        onClicked: {
            translationService.abort()
            visionService.abort()
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
        model: ["简体中文", "繁体中文", "English", "Japanese", "German", "Korean", "Español", "français"]
        // Restore the persisted target language; an empty/unknown stored
        // value keeps the default first entry.
        Component.onCompleted: {
            const idx = model.indexOf(setting.targetLanguage)
            if (idx >= 0) {
                currentIndex = idx
            }
        }
        // onActivated only fires for user picks, so restoring above does not
        // trigger a redundant config write.
        onActivated: {
            setting.targetLanguage = currentText
            setting.updateConfig()
        }
    }

    TranslationService{
        id:translationService
        onTranslationStarted: {
            result.text = ""
            root.stopped = false
            root.selectionUnavailable = false
        }
        onResponseDataChanged: {
            if (responseError === "") {
                result.text = responseData
            }
        }
        onResponseErrorChanged: {
            if (responseError !== "") {
                result.text = responseError
            }
        }
        onTranslationStopped: {
            // Keep the partial result visible, but distinguish it from Ready.
            root.stopped = true
        }
    }

    // Screenshot (multimodal) translation. Mirrors TranslationService
    // bindings; both services write into the same result panel, and starting
    // either session aborts the other first, so they never race.
    VisionTranslationService {
        id: visionService
        onTranslationStarted: {
            result.text = ""
            root.stopped = false
            root.selectionUnavailable = false
        }
        onResponseDataChanged: {
            if (responseError === "") {
                result.text = responseData
            }
        }
        onResponseErrorChanged: {
            if (responseError !== "") {
                result.text = responseError
            }
        }
        onTranslationStopped: {
            root.stopped = true
        }
    }
}

import QtQuick
import QtQuick.Controls
import ".."

// "Screenshot Translation" card: routing mode combo, vision-model override
// combo, capability warning and the advanced image options. Extracted from
// SettingView.qml; persistence stays in SettingView (edited/saveRequested
// signals, restore* functions). The screenshot hotkey lives in the Global
// Hotkey card (HotkeySection) together with the selection hotkey.
SectionCard {
    id: section
    title: I18n.t("Screenshot Translation")

    readonly property color mutedForeground: ThemeColors.mutedForeground
    readonly property color fieldColor: ThemeColors.fieldColor
    readonly property color fieldColorFocus: ThemeColors.fieldColorFocus
    readonly property color borderColor: ThemeColors.border
    readonly property color borderActive: ThemeColors.borderActive
    readonly property color warningColor: ThemeColors.warningText
    readonly property int fontSizeLabel: 13
    readonly property int fontSizeSmall: 14
    readonly property int fontSizeNormal: 15
    readonly property real radius: 10
    readonly property int controlHeight: 40

    // Stored vision-model override; "" = follow the current model.
    property string selectedVisionModel: ""
    // Screenshot routing mode: "auto" | "vision" | "ocr" (persisted as
    // setting.screenshotTranslateMode).
    property string translateMode: "auto"
    // The active provider's current model (for the capability warning).
    property string currentModel: ""
    // Detected models from the Model card (combo entries).
    property var detector: null
    property bool advancedExpanded: false

    // Debounced-save-worthy edits (advanced number fields).
    signal edited()
    // Immediate-save changes (mode / vision model combos).
    signal saveRequested()

    // Vision-model combo entries: "follow current" first, then the stored
    // override (when absent from the detected list) and the detected models.
    function visionModelChoices() {
        const choices = [I18n.t("Follow current model")]
        const stored = selectedVisionModel.trim()
        const detected = detector ? detector.availableModels : []
        if (stored.length > 0 && detected.indexOf(stored) === -1) {
            choices.push(stored)
        }
        return choices.concat(detected)
    }

    function restoreVisionModel(stored) {
        selectedVisionModel = stored
        const choices = visionModelChoices()
        visionModelCombo.currentIndex = Math.max(0, choices.indexOf(
            stored.trim().length > 0 ? stored.trim() : choices[0]))
    }

    function restoreAdvanced(maxImageWidth, jpegQuality) {
        maxWidthInput.text = String(maxImageWidth)
        jpegQualityInput.text = String(jpegQuality)
    }

    // Combo index order: Auto / Vision model / OCR.
    function restoreMode(mode) {
        translateMode = (mode === "vision" || mode === "ocr") ? mode : "auto"
        modeCombo.currentIndex = translateMode === "vision" ? 1 : (translateMode === "ocr" ? 2 : 0)
    }

    // Parsed advanced values; -1 when the current input is not acceptable.
    function maxImageWidth() {
        return maxWidthInput.acceptableInput ? parseInt(maxWidthInput.text) : -1
    }
    function jpegQuality() {
        return jpegQualityInput.acceptableInput ? parseInt(jpegQualityInput.text) : -1
    }

    SettingsRow {
        label: I18n.t("MODE")

        ThemedComboBox {
            id: modeCombo
            width: parent.width
            model: [I18n.t("Auto"), I18n.t("Vision model"), "OCR"]
            onActivated: function(index) {
                section.translateMode = index === 1 ? "vision" : (index === 2 ? "ocr" : "auto")
                section.saveRequested()
            }
        }
    }

    Text {
        width: parent.width
        leftPadding: section.fieldColumnX
        // OCR needs a backend: the bundled RapidOCR models on Linux/Windows,
        // the Vision framework on macOS (always available). The engine reports
        // the specific reason (missing models, tesseract not installed, ...).
        visible: section.translateMode === "ocr" && !ocrEngine.available
        text: ocrEngine.unavailableReason.length > 0
              ? I18n.t(ocrEngine.unavailableReason)
              : I18n.t("OCR is not available on this platform")
        color: section.warningColor
        font.pixelSize: section.fontSizeLabel
        wrapMode: Text.WordWrap
        lineHeight: 1.25
    }

    SettingsRow {
        label: I18n.t("VISION MODEL")

        ThemedComboBox {
            id: visionModelCombo
            width: parent.width
            model: section.visionModelChoices()
            onActivated: function(index) {
                section.selectedVisionModel = index === 0 ? "" : model[index]
                section.saveRequested()
            }
        }
    }

    Text {
        width: parent.width
        leftPadding: section.fieldColumnX
        readonly property string effectiveVisionModel: section.selectedVisionModel.trim().length > 0
            ? section.selectedVisionModel.trim() : section.currentModel.trim()
        visible: effectiveVisionModel.length > 0 && !setting.isLikelyVisionModel(effectiveVisionModel)
        text: I18n.t("This model may not support image input. Pick a vision-capable model above if screenshot translation fails.")
        color: section.warningColor
        font.pixelSize: section.fontSizeLabel
        wrapMode: Text.WordWrap
        lineHeight: 1.25
    }

    Text {
        text: (section.advancedExpanded ? "▾ " : "▸ ") + I18n.t("Advanced")
        color: advancedToggleArea.containsMouse ? section.accentHover : section.mutedForeground
        font.pixelSize: section.fontSizeSmall
        font.weight: Font.DemiBold
        Accessible.role: Accessible.Button
        Accessible.name: I18n.t("Advanced")
        Accessible.onPressAction: advancedToggleArea.clicked(null)

        MouseArea {
            id: advancedToggleArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: section.advancedExpanded = !section.advancedExpanded
        }
    }

    Column {
        width: parent.width
        spacing: 10
        visible: section.advancedExpanded

        SettingsRow {
            label: I18n.t("MAX IMAGE WIDTH")

            TextField {
                id: maxWidthInput
                width: 140
                height: section.controlHeight
                color: section.foreground
                font.pixelSize: section.fontSizeNormal
                leftPadding: 14
                rightPadding: 14
                verticalAlignment: TextInput.AlignVCenter
                validator: IntValidator { bottom: 512; top: 8192 }
                onTextChanged: section.edited()

                background: Rectangle {
                    color: maxWidthInput.activeFocus ? section.fieldColorFocus : section.fieldColor
                    radius: section.radius
                    border.width: 1
                    border.color: maxWidthInput.activeFocus ? section.borderActive : (maxWidthInput.hovered ? Qt.alpha(section.accent, 0.55) : section.borderColor)
                    Behavior on color { ColorAnimation { duration: 160 } }
                }
            }
        }

        SettingsRow {
            label: I18n.t("JPEG QUALITY")

            TextField {
                id: jpegQualityInput
                width: 140
                height: section.controlHeight
                color: section.foreground
                font.pixelSize: section.fontSizeNormal
                leftPadding: 14
                rightPadding: 14
                verticalAlignment: TextInput.AlignVCenter
                validator: IntValidator { bottom: 10; top: 100 }
                onTextChanged: section.edited()

                background: Rectangle {
                    color: jpegQualityInput.activeFocus ? section.fieldColorFocus : section.fieldColor
                    radius: section.radius
                    border.width: 1
                    border.color: jpegQualityInput.activeFocus ? section.borderActive : (jpegQualityInput.hovered ? Qt.alpha(section.accent, 0.55) : section.borderColor)
                    Behavior on color { ColorAnimation { duration: 160 } }
                }
            }
        }
    }
}

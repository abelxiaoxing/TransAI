import QtQuick
import QtQuick.Controls
import ".."

// "AI Provider" card: provider combo, API server, API key (with show/hide)
// and the key plausibility hint. Extracted from SettingView.qml; persistence
// and auto-detect orchestration stay in SettingView (edited signals).
SectionCard {
    id: section
    title: I18n.t("AI Provider")

    readonly property color mutedForeground: ThemeColors.mutedForeground
    readonly property color fieldColor: ThemeColors.fieldColor
    readonly property color fieldColorFocus: ThemeColors.fieldColorFocus
    readonly property color borderColor: ThemeColors.border
    readonly property color borderActive: ThemeColors.borderActive
    readonly property color warningColor: ThemeColors.warningText
    readonly property color errorColor: ThemeColors.error
    readonly property int fontSizeLabel: 13
    readonly property int fontSizeSmall: 14
    readonly property int fontSizeNormal: 15
    readonly property real radius: 10
    readonly property int controlHeight: 40

    property alias providerIndex: providerCombo.currentIndex
    property alias serverText: serverInput.text
    property alias keyText: keyInput.text
    // Controls the API-key row/hint visibility (Ollama needs no key).
    property bool ollama: false
    // Placeholder/first-launch value for the server field.
    property string defaultServerText: ""

    signal providerActivated(int index)
    // Text edits that should trigger a debounced save + auto model detect.
    signal serverEdited()
    signal keyEdited()

    SettingsRow {
        label: I18n.t("PROVIDER")

        ThemedComboBox {
            id: providerCombo
            width: parent.width
            model: [I18n.t("OpenAI / Compatible API"), I18n.t("Ollama (Local)")]
            // onActivated 仅在用户交互时触发；reload() 的程序化赋值不会走这里。
            onActivated: function(index) { section.providerActivated(index) }
        }
    }

    SettingsRow {
        label: I18n.t("API SERVER")

        TextField {
            id: serverInput
            width: parent.width
            height: section.controlHeight
            text: section.defaultServerText
            color: section.foreground
            font.pixelSize: section.fontSizeNormal
            leftPadding: 14
            rightPadding: 14
            verticalAlignment: TextInput.AlignVCenter
            onTextChanged: section.serverEdited()

            background: Rectangle {
                color: serverInput.activeFocus ? section.fieldColorFocus : section.fieldColor
                radius: section.radius
                border.width: 1
                border.color: serverInput.activeFocus ? section.borderActive : (serverInput.hovered ? Qt.alpha(section.accent, 0.55) : section.borderColor)
                Behavior on color { ColorAnimation { duration: 160 } }
            }
        }
    }

    SettingsRow {
        label: I18n.t("API KEY")
        visible: !section.ollama

        Item {
            width: parent.width
            height: section.controlHeight

            TextField {
                id: keyInput
                anchors.fill: parent
                echoMode: keyToggle.show ? TextInput.Normal : TextInput.Password
                passwordCharacter: "•"
                color: section.foreground
                font.pixelSize: section.fontSizeNormal
                leftPadding: 14
                rightPadding: 54
                verticalAlignment: TextInput.AlignVCenter
                onTextChanged: section.keyEdited()

                background: Rectangle {
                    color: keyInput.activeFocus ? section.fieldColorFocus : section.fieldColor
                    radius: section.radius
                    border.width: 1
                    border.color: keyInput.activeFocus ? section.borderActive : (keyInput.hovered ? Qt.alpha(section.accent, 0.55) : section.borderColor)
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
                color: keyToggleArea.containsMouse ? section.accentHover : section.mutedForeground
                font.pixelSize: section.fontSizeSmall
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
        leftPadding: section.fieldColumnX
        readonly property string trimmedKey: keyInput.text.trim()
        visible: !section.ollama && (trimmedKey.length === 0 || !setting.hasPlausibleApiKey(trimmedKey))
        text: trimmedKey.length === 0
              ? I18n.t("Required — paste the API key for your provider.")
              : I18n.t("This key doesn't look like a typical API key. Double-check it if requests fail.")
        color: trimmedKey.length === 0 ? section.errorColor : section.warningColor
        font.pixelSize: section.fontSizeLabel
        wrapMode: Text.WordWrap
    }
}

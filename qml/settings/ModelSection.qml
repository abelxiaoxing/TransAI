import QtQuick
import QtQuick.Controls
import TransAI
import ".."

// "Model" card: editable model field with auto-complete dropdown, server
// model detection (owns the ModelDiscoveryService) and the detect tip.
// Extracted from SettingView.qml; the detect orchestration (which server /
// key / provider to probe) stays in SettingView via detectRequested().
SectionCard {
    id: section
    title: I18n.t("Model")

    readonly property color mutedForeground: ThemeColors.mutedForeground
    readonly property color subtleForeground: ThemeColors.subtleForeground
    readonly property color fieldColor: ThemeColors.fieldColor
    readonly property color fieldColorFocus: ThemeColors.fieldColorFocus
    readonly property color panelColorAlt: ThemeColors.panelColorAlt
    readonly property color rowHighlightColor: ThemeColors.rowHighlightColor
    readonly property color borderColor: ThemeColors.border
    readonly property color borderActive: ThemeColors.borderActive
    readonly property color errorColor: ThemeColors.error
    readonly property int fontSizeSmall: 14
    readonly property int fontSizeNormal: 15
    readonly property real radius: 10
    readonly property int controlHeight: 40

    // Single source of truth for the model name: the input field itself.
    property alias modelText: modelInput.text
    // Read-only handle for the page header status pill / screenshot card.
    readonly property var detector: modelDetector
    // Whether a detect request is currently plausible (Ollama always,
    // OpenAI-compatible only with a plausible key). Computed by SettingView.
    property bool detectAllowed: false

    // 下拉过滤器：仅在弹窗展开期间由输入更新。不变式：弹窗关闭时恒为 ""
    // （onClosed 与每次打开前都会重置），因此点击打开总是展示完整列表。
    property string modelFilter: ""
    // modelPopup 最近一次关闭的时间戳（ms）。弹窗展开时点击输入框/箭头，按下会
    // 先通过 CloseOnPressOutside 关闭弹窗；该门控阻止同一次点击的抬起再把它打开，
    // 使点击表现为开关切换（无论弹窗遮罩是吃掉还是透传这次按下都成立）。
    property real modelPopupClosedAt: 0
    property bool openPopupAfterDetect: false

    // 下拉列表数据源：已探测模型按 modelFilter 做大小写不敏感的子串过滤
    readonly property var filteredModels: {
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

    // User typed in the model field (page applies the debounced save).
    signal edited()
    // A model was picked / committed (page saves immediately).
    signal saveRequested()
    // The detect button or an empty-list tap wants a server probe; the page
    // decides the server/key/provider arguments.
    signal detectRequested()

    function detect(server, key, provider) {
        modelDetector.detectModels(server, key, provider)
    }

    // 统一的下拉打开入口：先重置过滤器，保证点击打开时总是展示完整列表
    // （按过滤器不变式，弹窗正常关闭后此处本就为空，属于防御性重置）。
    function openModelPopup() {
        modelFilter = ""
        modelPopup.open()
    }

    function closePopup() {
        modelPopup.close()
    }

    // 点击模型输入框：列表就绪则直接展开；为空则触发一次探测，由
    // openPopupAfterDetect 在结果到达后自动展开。探测进行中、或
    // OpenAI key 明显不可用时不动作（页面已有对应的错误提示）。
    function handleModelFieldTap() {
        if (modelDetector.availableModels.length > 0) {
            openModelPopup()
        } else if (!modelDetector.isDetecting && section.detectAllowed) {
            section.openPopupAfterDetect = true
            section.detectRequested()
        }
    }

    ModelDiscoveryService {
        id: modelDetector
        onAvailableModelsChanged: {
            if (section.openPopupAfterDetect && availableModels.length > 0) {
                section.openModelPopup()
                section.openPopupAfterDetect = false
            }
        }
        onIsDetectingChanged: {
            if (!isDetecting && availableModels.length === 0) {
                section.openPopupAfterDetect = false
            }
        }
    }

    headerData: Row {
        width: parent ? parent.width : 0
        spacing: 8

        Item {
            id: modelRow
            width: parent.width - detectButton.width - 8
            height: section.controlHeight

            TextField {
                id: modelInput
                anchors.fill: parent
                // modelText 是此 text 属性的别名（唯一数据源）。
                color: section.foreground
                font.pixelSize: section.fontSizeNormal
                leftPadding: 14
                rightPadding: 34
                onTextChanged: {
                    section.edited()
                    // 下拉展开时输入即过滤（自动补全）；activeFocus 门控
                    // 避免程序化回填（reload / 切换 provider / 选中项）改动过滤器。
                    if (modelPopup.opened && activeFocus) {
                        section.modelFilter = text
                    }
                }

                // Enter 直接采纳过滤结果：优先完全匹配，否则取第一条。
                Keys.onPressed: (event) => {
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                        && modelPopup.opened && section.filteredModels.length > 0) {
                        const exact = section.filteredModels.indexOf(text.trim())
                        modelInput.text = exact >= 0 ? section.filteredModels[exact] : section.filteredModels[0]
                        modelPopup.close()
                        section.saveRequested()
                        event.accepted = true
                    }
                }

                // 可编辑下拉框交互：点击输入框即展开下拉；列表为空时改为触发
                // 一次探测并在结果到达后自动展开。弹窗展开时同一次点击的按下会
                // 先被 CloseOnPressOutside 用来关闭弹窗，时间戳门控保证抬起不会
                // 再打开，因此点击表现为开关切换。
                TapHandler {
                    onTapped: {
                        if (Date.now() - section.modelPopupClosedAt > 250) {
                            section.handleModelFieldTap()
                        }
                    }
                }

                background: Rectangle {
                    color: modelInput.activeFocus || modelPopup.opened ? section.fieldColorFocus : section.fieldColor
                    radius: section.radius
                    border.width: 1
                    border.color: modelPopup.opened ? section.borderActive : (modelInput.hovered || modelInput.activeFocus ? Qt.alpha(section.accent, 0.55) : section.borderColor)
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
                    color: section.subtleForeground
                    font.pixelSize: section.fontSizeNormal
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
                color: modelPopup.opened || modelArrowArea.containsMouse ? section.accentHover : section.mutedForeground
                font.pixelSize: section.fontSizeNormal

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
                        } else if (Date.now() - section.modelPopupClosedAt > 250) {
                            section.openModelPopup()
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
                implicitHeight: section.filteredModels.length === 0 ? 40 : Math.min(modelList.contentHeight + 2, listMaxHeight)
                padding: 1
                // 不抢键盘焦点：弹窗展开时焦点留在输入框，打字继续过滤
                focus: false
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                onAboutToShow: {
                    // Window-level coordinates: the page fills the window, so
                    // mapping to null yields the same space math as before.
                    const rowY = modelRow.mapToItem(null, 0, 0).y
                    const win = modelRow.Window.window
                    const winHeight = win ? win.height : 700
                    const below = winHeight - rowY - modelRow.height - 12
                    const above = rowY - 12
                    const desired = Math.min(modelList.contentHeight + 2, 260)
                    openUpward = below < desired && above > below
                    listMaxHeight = Math.max(102, Math.min(260, openUpward ? above : below))
                    // 长列表中把当前选中的模型滚动到可见位置；
                    // 打开前过滤器已重置，filteredModels 即完整列表
                    const current = section.filteredModels.indexOf(modelInput.text.trim())
                    if (current >= 0) {
                        modelList.positionViewAtIndex(current, ListView.Center)
                    }
                }

                // 关闭时恢复过滤器不变式，并记录时间戳供点击门控使用
                onClosed: {
                    section.modelFilter = ""
                    section.modelPopupClosedAt = Date.now()
                }

                contentItem: Item {
                    ListView {
                        id: modelList
                        anchors.fill: parent
                        clip: true
                        model: section.filteredModels
                        ScrollIndicator.vertical: ScrollIndicator { }

                        delegate: ItemDelegate {
                            width: modelList.width
                            height: 34
                            hoverEnabled: true
                            // 选项不抢焦点：点选后键盘焦点留在输入框，可继续输入
                            focusPolicy: Qt.NoFocus

                            contentItem: Text {
                                text: modelData
                                color: modelInput.text.trim() === modelData ? section.accent : section.foreground
                                font.pixelSize: section.fontSizeNormal
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                leftPadding: 12
                                rightPadding: 12
                            }

                            background: Rectangle {
                                color: hovered ? section.rowHighlightColor : section.panelColorAlt
                            }

                            onClicked: {
                                // 先关弹窗再赋值：赋值会触发 modelInput.onTextChanged，
                                // 若弹窗仍开着会更新 modelFilter → ListView model 变化 →
                                // 本委托被销毁，后续语句抛 ReferenceError。
                                const picked = modelData
                                modelPopup.close()
                                modelInput.text = picked
                                section.saveRequested()
                            }
                        }
                    }

                    // 过滤无结果时的空态提示（此时手动输入仍然有效）
                    Text {
                        anchors.centerIn: parent
                        visible: section.filteredModels.length === 0
                        text: I18n.t("No matching models")
                        color: section.subtleForeground
                        font.pixelSize: section.fontSizeSmall
                    }
                }

                background: Rectangle {
                    color: section.panelColorAlt
                    radius: section.radius
                    border.width: 1
                    border.color: section.borderActive
                }
            }
        }

        Rectangle {
            id: detectButton
            width: 92
            height: section.controlHeight
            radius: section.radius
            color: detectArea.containsMouse && !modelDetector.isDetecting ? section.accentHover : section.accent
            opacity: modelDetector.isDetecting ? 0.65 : 1.0
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.18)

            Text {
                anchors.centerIn: parent
                text: modelDetector.isDetecting ? I18n.t("Scanning")
                      : modelDetector.availableModels.length > 0 ? I18n.t("Refresh")
                      : I18n.t("Detect")
                color: "#061014"
                font.pixelSize: section.fontSizeSmall
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: detectArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: modelDetector.isDetecting ? Qt.ArrowCursor : Qt.PointingHandCursor
                onClicked: {
                    if (!modelDetector.isDetecting) {
                        section.openPopupAfterDetect = true
                        section.detectRequested()
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
        color: modelDetector.error !== "" ? Qt.alpha(section.errorColor, 0.10) : Qt.alpha(section.accent, 0.09)
        border.width: 1
        border.color: modelDetector.error !== "" ? Qt.alpha(section.errorColor, 0.35) : Qt.alpha(section.accent, 0.28)
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
            color: modelDetector.error !== "" ? section.errorColor : section.mutedForeground
            font.pixelSize: section.fontSizeSmall
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }
}

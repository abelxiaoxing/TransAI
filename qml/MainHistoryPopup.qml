import QtQuick
import QtQuick.Controls
import "."

// Recent-translations popup (newest first). Reads the "translationHistory"
// context property directly; the owning view decides what selecting an entry
// restores (via entryActivated) and closes the popup.
//
// Lives in the window scene like any other in-window popup, so main.qml's
// click-outside logic treats it as "inside the window".
Popup {
    id: popup

    // The selected history entry (a QVariantMap: type/source/result/...).
    signal entryActivated(var entry)

    x: 10
    y: 46
    width: parent ? parent.width - 20 : 0
    height: Math.min(parent ? parent.height - 90 : 0, contentColumn.implicitHeight + 20)
    padding: 10
    modal: false
    dim: false
    background: Rectangle {
        radius: 8
        color: ThemeColors.panelColor
        border.width: 1
        border.color: ThemeColors.borderActive
    }

    contentItem: Column {
        id: contentColumn
        spacing: 8

        Item {
            width: contentColumn.width
            height: 22

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: I18n.t("History")
                color: ThemeColors.accent
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: translationHistory.entries.length > 0
                text: I18n.t("Clear")
                color: clearArea.containsMouse ? ThemeColors.accentHover : ThemeColors.mutedForeground
                font.pixelSize: 12
                font.weight: Font.DemiBold
                MouseArea {
                    id: clearArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: translationHistory.clear()
                }
            }
        }

        Text {
            visible: translationHistory.entries.length === 0
            width: contentColumn.width
            horizontalAlignment: Text.AlignHCenter
            text: I18n.t("No history yet")
            color: ThemeColors.mutedForeground
            font.pixelSize: 12
            topPadding: 18
            bottomPadding: 18
        }

        ListView {
            id: entryList
            visible: translationHistory.entries.length > 0
            width: contentColumn.width
            height: Math.min(contentHeight, popup.parent ? popup.parent.height - 160 : contentHeight)
            clip: true
            spacing: 6
            model: translationHistory.entries

            ScrollBar.vertical: ScrollBar {
                policy: entryList.contentHeight > entryList.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                width: 6
            }

            delegate: Rectangle {
                required property var modelData
                width: entryList.width
                height: entryColumn.implicitHeight + 14
                radius: 6
                color: entryArea.containsMouse ? Qt.alpha(ThemeColors.accent, 0.14) : ThemeColors.panelColorAlt
                border.width: 1
                border.color: ThemeColors.border

                Column {
                    id: entryColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 8
                    spacing: 3

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        // Screenshot entries keep the recognized text as their
                        // source (when OCR produced one); the type marker keeps
                        // them distinguishable.
                        text: modelData.type === "screenshot"
                              ? (modelData.source.length > 0
                                 ? I18n.t("[Screenshot]") + " " + modelData.source.replace(/\s+/g, " ")
                                 : I18n.t("[Screenshot]"))
                              : modelData.source.replace(/\s+/g, " ")
                        color: ThemeColors.mutedForeground
                        font.pixelSize: 12
                    }

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                        text: modelData.result
                        color: ThemeColors.foreground
                        font.pixelSize: 12
                    }
                }

                MouseArea {
                    id: entryArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: popup.entryActivated(modelData)
                }
            }
        }
    }
}

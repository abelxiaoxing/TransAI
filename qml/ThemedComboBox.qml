import QtQuick
import QtQuick.Controls

// Shared dark-theme ComboBox for the settings page. Callers provide the
// model and react to index changes; all styling lives here.
ComboBox {
    id: control

    height: 40
    font.pixelSize: 15
    hoverEnabled: true

    background: Rectangle {
        color: control.hovered || control.popup.opened ? ThemeColors.fieldColorFocus : ThemeColors.fieldColor
        radius: 10
        border.width: 1
        border.color: control.popup.opened ? ThemeColors.borderActive
                                           : (control.hovered ? Qt.alpha(ThemeColors.accent, 0.55) : ThemeColors.border)
        Behavior on color { ColorAnimation { duration: 160 } }
    }

    contentItem: Text {
        text: control.displayText
        color: ThemeColors.foreground
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        leftPadding: 14
        rightPadding: 40
    }

    indicator: Text {
        x: control.width - width - 14
        y: (control.height - height) / 2
        text: control.popup.opened ? "▴" : "▾"
        color: control.hovered || control.popup.opened ? ThemeColors.accentHover : ThemeColors.mutedForeground
        font.pixelSize: 14
    }

    delegate: ItemDelegate {
        width: control.width
        height: 34
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: modelData
            color: control.currentIndex === index ? ThemeColors.accent : ThemeColors.foreground
            font: control.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            leftPadding: 12
            rightPadding: 12
        }

        background: Rectangle {
            color: control.highlightedIndex === index ? ThemeColors.rowHighlightColor : ThemeColors.panelColorAlt
            radius: 8
        }
    }

    popup: Popup {
        y: control.height + 6
        width: control.width
        implicitHeight: contentItem.implicitHeight + 2
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: ThemeColors.panelColorAlt
            radius: 10
            border.width: 1
            border.color: ThemeColors.borderActive
        }
    }
}

import QtQuick

// Shared page background: dark gradient plus two subtle neon ambience
// circles. Used by AppView and SettingView so the visual shell stays
// identical across pages.
Item {
    id: root

    // Vertical offset of the purple accent circle; pages may nudge it.
    property int accentCircleTopMargin: 110

    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: ThemeColors.background
        gradient: Gradient {
            GradientStop { position: 0.0; color: ThemeColors.gradientTop }
            GradientStop { position: 0.52; color: ThemeColors.background }
            GradientStop { position: 1.0; color: ThemeColors.gradientBottom }
        }
    }

    Rectangle {
        width: 260
        height: 260
        radius: 130
        x: -120
        y: -90
        color: Qt.alpha(ThemeColors.accent, 0.12)
        border.color: Qt.alpha(ThemeColors.accent, 0.18)
        border.width: 1
    }

    Rectangle {
        width: 220
        height: 220
        radius: 110
        anchors.right: parent.right
        anchors.rightMargin: -110
        anchors.top: parent.top
        anchors.topMargin: root.accentCircleTopMargin
        color: Qt.alpha(ThemeColors.accentPurple, 0.10)
        border.color: Qt.alpha(ThemeColors.accentPurple, 0.16)
        border.width: 1
    }
}

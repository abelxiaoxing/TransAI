pragma Singleton
import QtQml
import QtQuick

QtObject {
    // Single source of truth for the app-wide dark palette and shared design
    // tokens. Registered as a singleton in qmldir, available to all QML files
    // in this directory via the implicit import.

    // Backgrounds
    readonly property color background: "#090D13"
    readonly property color panelColor: "#141F2A"
    readonly property color panelColorAlt: "#101720"
    readonly property color fieldColor: "#0D141D"
    // Focused input background and highlighted list row (shared by the
    // settings page, ThemedComboBox and the model popup).
    readonly property color fieldColorFocus: "#152332"
    readonly property color rowHighlightColor: "#1A2A3A"
    // Settings card surface.
    readonly property color cardColor: Qt.rgba(0.07, 0.11, 0.16, 0.82)
    readonly property color gradientTop: "#101827"
    readonly property color gradientBottom: "#070A10"

    // Text
    readonly property color foreground: "#E8F3F7"
    readonly property color mutedForeground: "#89A2B2"
    readonly property color subtleForeground: "#5F7484"

    // Accents
    readonly property color accent: "#65F4D6"
    readonly property color accentHover: "#8CFFE8"
    readonly property color accentPurple: "#7C5CFF"

    // Borders
    readonly property color border: "#243445"
    readonly property color borderActive: "#65F4D6"
    // Idle border of the main-view input panel (bluish, slightly transparent).
    readonly property color borderIdle: Qt.rgba(0.24, 0.4, 0.57, 0.35)
    // Decorative 1px separator / inner card outline.
    readonly property color hairline: Qt.rgba(1, 1, 1, 0.04)

    // Text/icons placed on top of an accent-colored surface (buttons, selected
    // segments).
    readonly property color accentForeground: "#0F1721"
    readonly property color accentForegroundDisabled: "#A3B0C0"

    // Small chips (settings status pills).
    readonly property color chipBg: Qt.rgba(1, 1, 1, 0.055)
    readonly property color chipBorder: Qt.rgba(1, 1, 1, 0.08)

    // Round control button states (settings back button).
    readonly property color controlDown: "#0C1219"
    readonly property color controlHover: "#182838"
    readonly property color controlIdle: Qt.rgba(0.07, 0.11, 0.16, 0.76)

    // Popup/dropdown chrome.
    readonly property color popupBorder: Qt.rgba(1, 1, 1, 0.18)
    readonly property color popupSelectedText: "#061014"

    // Semantic
    readonly property color error: "#F48771"
    readonly property color infoBg: "#122033"
    readonly property color warningBg: "#2D1D2F"
    readonly property color infoText: "#8DD9FF"
    readonly property color warningText: "#FF9BAA"
}

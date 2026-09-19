pragma Singleton
import QtQml
import QtQuick

QtObject {
    // Single source of truth for the app-wide dark palette and shared design
    // tokens. Registered as a singleton in qmldir, available to all QML files
    // in this directory via the implicit import.

    // Backgrounds
    readonly property color background: "#090D13"
    readonly property color backgroundSecondary: "#121A24"
    readonly property color panelColor: "#141F2A"
    readonly property color panelColorAlt: "#101720"
    readonly property color fieldColor: "#0D141D"
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

    // Semantic
    readonly property color error: "#F48771"
    readonly property color success: "#4EC9B0"
    readonly property color warning: "#FFD166"
    readonly property color infoBg: "#122033"
    readonly property color warningBg: "#2D1D2F"
    readonly property color infoText: "#8DD9FF"
    readonly property color warningText: "#FF9BAA"

    // Motion
    readonly property int animationDuration: 250
    readonly property int animationDurationFast: 80
}

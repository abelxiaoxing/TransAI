import QtQuick
import QtQuick.Controls
import ".."

// "Language" card: UI display-language combo. Extracted from SettingView.qml;
// the index ↔ language-code mapping stays in SettingView (reload/saveConfig).
SectionCard {
    id: section
    title: I18n.t("Language")

    property alias languageIndex: languageCombo.currentIndex
    // User picked an entry (onActivated never fires for programmatic
    // restores, so this needs no ready-gate).
    signal languageActivated()

    SettingsRow {
        label: I18n.t("DISPLAY LANGUAGE")

        ThemedComboBox {
            id: languageCombo
            width: parent.width
            // Language names always stay in their native language.
            model: ["English", "简体中文"]
            onActivated: section.languageActivated()
        }
    }
}

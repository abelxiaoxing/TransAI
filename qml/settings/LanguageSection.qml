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

    headerData: ThemedComboBox {
        id: languageCombo
        // parent is the card's headerArea (via the headerData alias).
        width: parent ? parent.width : 0
        // Language names always stay in their native language.
        model: ["English", "简体中文"]
        onActivated: section.languageActivated()
    }
}

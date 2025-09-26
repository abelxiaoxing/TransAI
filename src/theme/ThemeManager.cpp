#include "ThemeManager.h"
#include <QQuickWindow>
#include <QGuiApplication>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_currentTheme(DarkTheme)
    , m_settings(new QSettings("TransAI", "ThemeManager", this))
{
    initializeColorPalette();
    loadTheme();
}

ThemeManager::~ThemeManager()
{
}

ThemeManager::Theme ThemeManager::currentTheme() const
{
    return m_currentTheme;
}

void ThemeManager::setCurrentTheme(Theme theme)
{
    if (m_currentTheme != theme) {
        m_currentTheme = theme;
        initializeColorPalette();
        emit currentThemeChanged(theme);
        saveTheme();
    }
}

QColor ThemeManager::getColor(const QString& colorName) const
{
    return m_colorPalette.value(colorName, QColor("#000000"));
}

void ThemeManager::applyTheme(QQuickWindow* window)
{
    if (!window) return;

    // 应用主题到窗口
    switch (m_currentTheme) {
    case DarkTheme:
        window->setColor(getColor("background"));
        break;
    case LightTheme:
        window->setColor(getColor("background"));
        break;
    }
}

void ThemeManager::saveTheme()
{
    m_settings->setValue("theme", static_cast<int>(m_currentTheme));
}

void ThemeManager::loadTheme()
{
    bool ok;
    int themeValue = m_settings->value("theme", static_cast<int>(DarkTheme)).toInt(&ok);
    if (ok) {
        m_currentTheme = static_cast<Theme>(themeValue);
        initializeColorPalette();
    }
}

void ThemeManager::initializeColorPalette()
{
    m_colorPalette.clear();

    switch (m_currentTheme) {
    case DarkTheme:
        initializeDarkThemeColors();
        break;
    case LightTheme:
        initializeLightThemeColors();
        break;
    }
}

void ThemeManager::initializeDarkThemeColors()
{
    // 深色主题配色方案
    m_colorPalette["background"] = QColor("#1E1E1E");
    m_colorPalette["backgroundSecondary"] = QColor("#252526");
    m_colorPalette["foreground"] = QColor("#D4D4D4");
    m_colorPalette["foregroundSecondary"] = QColor("#969696");
    m_colorPalette["accent"] = QColor("#4EC9B0");
    m_colorPalette["accentHover"] = QColor("#5ED9C0");
    m_colorPalette["border"] = QColor("#3E3E42");
    m_colorPalette["shadow"] = QColor("rgba(0, 0, 0, 0.2)");
}

void ThemeManager::initializeLightThemeColors()
{
    // 浅色主题配色方案（为未来扩展预留）
    m_colorPalette["background"] = QColor("#FFFFFF");
    m_colorPalette["backgroundSecondary"] = QColor("#F5F5F5");
    m_colorPalette["foreground"] = QColor("#1E1E1E");
    m_colorPalette["foregroundSecondary"] = QColor("#666666");
    m_colorPalette["accent"] = QColor("#4EC9B0");
    m_colorPalette["accentHover"] = QColor("#5ED9C0");
    m_colorPalette["border"] = QColor("#E0E0E0");
    m_colorPalette["shadow"] = QColor("rgba(0, 0, 0, 0.1)");
}
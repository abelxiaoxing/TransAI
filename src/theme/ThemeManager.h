#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <QObject>
#include <QColor>
#include <QHash>
#include <QSettings>
#include <QQuickWindow>

class ThemeManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Theme currentTheme READ currentTheme WRITE setCurrentTheme NOTIFY currentThemeChanged)

public:
    enum Theme {
        DarkTheme,
        LightTheme    // 为未来扩展预留
    };
    Q_ENUM(Theme)

    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager();

    Theme currentTheme() const;
    void setCurrentTheme(Theme theme);

    Q_INVOKABLE QColor getColor(const QString& colorName) const;
    Q_INVOKABLE void applyTheme(QQuickWindow* window);
    Q_INVOKABLE void saveTheme();
    Q_INVOKABLE void loadTheme();

signals:
    void currentThemeChanged(Theme theme);

private:
    Theme m_currentTheme;
    QHash<QString, QColor> m_colorPalette;
    QSettings* m_settings;

    void initializeColorPalette();
    void initializeDarkThemeColors();
    void initializeLightThemeColors();
};

#endif // THEME_MANAGER_H
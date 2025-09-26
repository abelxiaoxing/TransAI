# 深色主题系统架构

## 主题管理器设计

```cpp
// 主题管理器架构
class ThemeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(Theme currentTheme READ currentTheme WRITE setCurrentTheme NOTIFY currentThemeChanged)

public:
    enum Theme {
        DarkTheme,
        LightTheme    // 为未来扩展预留
    };

    Q_INVOKABLE QColor getColor(const QString& colorName) const;
    Q_INVOKABLE void applyTheme(QQuickWindow* window);

private:
    Theme m_currentTheme;
    QHash<QString, QColor> m_colorPalette;
};
```

## 颜色系统定义

```qml
// ThemeColors.qml
pragma Singleton

QtObject {
    // 深色主题配色
    readonly property color background: "#1E1E1E"
    readonly property color backgroundSecondary: "#252526"
    readonly property color foreground: "#D4D4D4"
    readonly property color foregroundSecondary: "#969696"
    readonly property color accent: "#4EC9B0"
    readonly property color accentHover: "#5ED9C0"
    readonly property color border: "#3E3E42"
    readonly property color shadow: "rgba(0, 0, 0, 0.2)"

    // 统一的设计令牌
    readonly property real radius: 8
    readonly property real spacing: 8
    readonly property real animationDuration: 250
}
```

## 主题应用策略

**组件级主题应用：**
- 所有 QML 组件通过主题管理器获取颜色和样式
- 使用 Qt 的属性绑定系统实现动态主题切换
- 保持向后兼容，不影响现有组件

**性能优化考虑：**
- 颜色值缓存，避免重复计算
- 使用 Qt 的批处理渲染优化
- 动画效果使用 GPU 加速
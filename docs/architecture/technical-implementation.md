# 技术实现细节

## 代码组织结构

```
src/
├── theme/
│   ├── ThemeManager.h/cpp          # 主题管理器
│   └── ThemeColors.qml             # 主题颜色定义
├── ui/
│   ├── GTextEdit.qml               # 改进的输入框组件
│   ├── TranslationPopup.qml        # 翻译弹窗
│   └── StyledComponent.qml         # 基础样式组件
├── translation/
│   ├── TextSelectionDetector.h/cpp # 文本选择检测
│   ├── SelectionTranslationService.h/cpp # 划词翻译服务
│   └── HotkeyManager.h/cpp         # 热键管理器（现有）
├── animation/
│   └── AnimationManager.qml        # 动画管理器
└── Controller.h/cpp                # 主控制器（现有）
```

## 依赖关系管理

**主题系统依赖：**
- `ThemeManager` → `Controller` (配置管理)
- `ThemeColors` → 所有 QML 组件
- `StyledComponent` → `ThemeColors`

**划词翻译依赖：**
- `SelectionTranslationService` → `TextSelectionDetector`
- `SelectionTranslationService` → `HotkeyManager`
- `SelectionTranslationService` → `Controller`
- `TranslationPopup` → `ThemeColors`

## 配置管理

```cpp
// Settings.h - 配置扩展
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(Theme theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool wordSelectionEnabled READ wordSelectionEnabled WRITE setWordSelectionEnabled NOTIFY wordSelectionEnabled)
    Q_PROPERTY(QKeySequence wordSelectionHotkey READ wordSelectionHotkey WRITE setWordSelectionHotkey)

public:
    enum Theme {
        DarkTheme,
        LightTheme
    };

    Theme theme() const;
    void setTheme(Theme theme);

    bool wordSelectionEnabled() const;
    void setWordSelectionEnabled(bool enabled);

    QKeySequence wordSelectionHotkey() const;
    void setWordSelectionHotkey(const QKeySequence& hotkey);
};
```
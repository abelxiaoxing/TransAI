# TransAI UI/UX 改进技术架构文档

## 📋 变更记录

| 日期 | 版本 | 描述 | 作者 |
|------|------|------|------|
| 2025-09-26 | v1.0 | 初始架构设计 - UI/UX 改进 | 架构师 |

---

## 🎯 架构目标

基于 PRD 中的功能需求，本架构设计旨在：

1. **保持现有架构稳定性** - 在现有 Qt/QML 架构基础上进行改进
2. **提升用户体验** - 实现深色主题、优化交互、添加划词翻译功能
3. **确保跨平台兼容性** - 在 Windows、macOS、Linux 上保持一致体验
4. **优化性能** - 所有 UI 改进不得影响应用性能

---

## 🏗️ 整体架构

### 架构概述

```
TransAI 应用架构
├── 前端层 (Frontend Layer)
│   ├── QML 界面组件
│   ├── 深色主题系统
│   ├── 交互控制器
│   └── 动画效果管理
├── 业务逻辑层 (Business Logic Layer)
│   ├── 翻译控制器 (Controller)
│   ├── 热键管理器 (Hotkey)
│   ├── 划词翻译服务
│   └── 主题管理器
├── 数据层 (Data Layer)
│   ├── 配置管理 (Settings)
│   ├── 主题配置
│   └── 用户偏好
└── 系统集成层 (Integration Layer)
    ├── 系统剪贴板
    ├── 全局热键
    ├── 文本选择检测
    └── OpenAI API
```

### 技术栈

- **前端**: Qt 6 + QML + Qt Quick Controls
- **后端**: C++17 + Qt Framework
- **构建**: CMake 3.16+
- **第三方库**: QHotkey (热键功能)
- **API**: OpenAI GPT API

---

## 🎨 深色主题系统架构

### 主题管理器设计

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

### 颜色系统定义

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

### 主题应用策略

**组件级主题应用：**
- 所有 QML 组件通过主题管理器获取颜色和样式
- 使用 Qt 的属性绑定系统实现动态主题切换
- 保持向后兼容，不影响现有组件

**性能优化考虑：**
- 颜色值缓存，避免重复计算
- 使用 Qt 的批处理渲染优化
- 动画效果使用 GPU 加速

---

## 🔧 输入框交互优化架构

### GTextEdit 组件重构

```qml
// GTextEdit.qml 改进版本
Flickable {
    id: flickable

    // 全局鼠标区域解决焦点问题
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true

        onClicked: {
            if (mouse.button === Qt.LeftButton) {
                // 计算点击位置对应的文本位置
                var pos = textEdit.positionAt(mouse.x, mouse.y + flickable.contentY);
                textEdit.forceActiveFocus();
                textEdit.cursorPosition = pos;
                mouse.accepted = true;
            } else {
                mouse.accepted = false;
            }
        }
    }

    TextEdit {
        id: textEdit
        // 保持现有功能不变
        wrapMode: Text.WrapAnywhere
        selectByMouse: true
        selectByKeyboard: true

        // 应用深色主题
        color: ThemeColors.foreground
        selectedTextColor: ThemeColors.background
        selectionColor: ThemeColors.accent
    }
}
```

### 事件处理流程

```
用户点击 → MouseArea 捕获 → 计算文本位置 → 设置焦点 → 移动光标 → 完成交互
```

### 兼容性保证

- 保持现有 API 接口不变
- 保持现有信号和槽机制
- 保持现有文本选择和编辑功能
- 保持滚动功能正常工作

---

## 🎯 划词翻译功能架构

### 跨平台文本选择检测

```cpp
// TextSelectionDetector.h
class TextSelectionDetector : public QObject {
    Q_OBJECT

public:
    explicit TextSelectionDetector(QObject* parent = nullptr);
    void startMonitoring();
    void stopMonitoring();

signals:
    void textSelected(const QString& text);

private:
#ifdef Q_OS_WIN
    void monitorWindowsSelection();
#elif defined(Q_OS_MACOS)
    void monitorMacSelection();
#else
    void monitorLinuxSelection();
#endif

    QString getSelectedText();
    QTimer* m_monitorTimer;
};
```

### 跨平台实现策略

**Windows 实现：**
- 使用 Windows API 监听剪贴板变化
- 通过 `GetClipboardData()` 获取选中文本
- 使用 `SetClipboardViewer()` 监听剪贴板事件

**macOS 实现：**
- 使用 Cocoa 框架的 `NSPasteboard`
- 通过 `NSAppleEventManager` 监听系统事件
- 使用 Accessibility API 获取选中文本

**Linux 实现：**
- 使用 X11 库的 `XSelectionEvent`
- 通过 `QClipboard` 监听选择变化
- 支持多种桌面环境 (GNOME, KDE, XFCE)

### 划词翻译服务

```cpp
// SelectionTranslationService.h
class SelectionTranslationService : public QObject {
    Q_OBJECT

public:
    explicit SelectionTranslationService(Controller* controller, QObject* parent = nullptr);

    void triggerTranslation(const QString& text, const QPoint& cursorPos);

private slots:
    void onSelectionDetected(const QString& text);
    void onHotkeyTriggered();

private:
    TextSelectionDetector* m_detector;
    HotkeyManager* m_hotkeyManager;
    TranslationPopup* m_popupWindow;
    Controller* m_controller;
};
```

### 翻译弹窗架构

```qml
// TranslationPopup.qml
ApplicationWindow {
    id: popupWindow

    // 点击外部自动关闭
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true

        onClicked: {
            var clickPos = mapToItem(null, mouse.x, mouse.y);
            if (!popupWindow.contains(clickPos)) {
                popupWindow.close();
            }
        }
    }

    // 智能定位逻辑
    function showAtPosition(text, globalPos) {
        var screenRect = Screen.availableGeometry;
        var windowSize = Qt.size(400, 300);

        // 计算窗口位置，避免遮挡选中文本
        var x = globalPos.x + 20;
        var y = globalPos.y + 20;

        // 边界检查
        if (x + windowSize.width > screenRect.right) {
            x = globalPos.x - windowSize.width - 20;
        }
        if (y + windowSize.height > screenRect.bottom) {
            y = globalPos.y - windowSize.height - 20;
        }

        popupWindow.x = x;
        popupWindow.y = y;
        popupWindow.show();
    }
}
```

---

## 🎨 界面精美化架构

### 统一视觉系统

```qml
// StyledComponent.qml - 基础样式组件
Rectangle {
    id: root

    // 统一的样式属性
    color: ThemeColors.background
    radius: ThemeColors.radius
    border.color: ThemeColors.border
    border.width: 1

    // 阴影效果
    layer.enabled: true
    layer.effect: RectangularGlow {
        glowRadius: 4
        spread: 0.2
        color: ThemeColors.shadow
        cornerRadius: root.radius
    }

    // 悬停效果
    Behavior on color {
        ColorAnimation {
            duration: ThemeColors.animationDuration
            easing.type: Easing.OutQuad
        }
    }
}
```

### 动画系统

```qml
// AnimationManager.qml - 动画管理器
QtObject {
    id: animationManager

    // 统一的动画时长
    readonly property int fastDuration: 150
    readonly property int normalDuration: 250
    readonly property int slowDuration: 350

    // 通用动画组件
    function fadeTransition(target, from, to, duration = normalDuration) {
        return PropertyAnimation {
            target: target
            property: "opacity"
            from: from
            to: to
            duration: duration
            easing.type: Easing.OutQuad
        }
    }

    function scaleTransition(target, from, to, duration = normalDuration) {
        return PropertyAnimation {
            target: target
            property: "scale"
            from: from
            to: to
            duration: duration
            easing.type: Easing.OutQuad
        }
    }
}
```

### 性能优化策略

**渲染优化：**
- 使用 `layer.enabled` 启用硬件加速
- 避免复杂的 Shader 效果
- 合理使用 `visible` 属性控制渲染

**动画优化：**
- 使用 Qt 内置的动画系统
- 避免同时运行过多动画
- 使用 `pause()` 和 `resume()` 控制动画状态

**内存优化：**
- 及时销毁不用的组件
- 使用 `Loader` 延迟加载非关键组件
- 合理使用 `Qt.createComponent()`

---

## 🔧 技术实现细节

### 代码组织结构

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

### 依赖关系管理

**主题系统依赖：**
- `ThemeManager` → `Controller` (配置管理)
- `ThemeColors` → 所有 QML 组件
- `StyledComponent` → `ThemeColors`

**划词翻译依赖：**
- `SelectionTranslationService` → `TextSelectionDetector`
- `SelectionTranslationService` → `HotkeyManager`
- `SelectionTranslationService` → `Controller`
- `TranslationPopup` → `ThemeColors`

### 配置管理

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

---

## 🚀 性能优化策略

### 渲染性能

**优化目标：**
- 保持 60fps 的流畅动画
- 避免界面卡顿和延迟
- 降低 CPU 和 GPU 占用

**优化措施：**
1. **批量渲染**: 使用 Qt 的批处理渲染机制
2. **硬件加速**: 启用 OpenGL 加速
3. **组件复用**: 避免重复创建相同组件
4. **懒加载**: 非关键组件延迟加载

### 内存管理

**内存优化目标：**
- 控制应用内存占用在 100MB 以内
- 避免内存泄漏
- 及时释放不用的资源

**优化措施：**
1. **对象池**: 复用频繁创建销毁的对象
2. **资源管理**: 使用 Qt 的资源系统管理图片和样式
3. **及时清理**: 在组件销毁时清理资源

### 启动性能

**启动优化目标：**
- 冷启动时间 < 3 秒
- 热启动时间 < 1 秒

**优化措施：**
1. **延迟加载**: 非核心功能延迟初始化
2. **后台预加载**: 在应用空闲时预加载资源
3. **配置缓存**: 缓存用户配置减少 I/O 操作

---

## 🧪 测试策略

### 单元测试

**测试重点：**
- 主题管理器的颜色计算
- 文本选择检测的准确性
- 热键绑定的正确性
- 配置管理的持久化

```cpp
// TestThemeManager.cpp
class TestThemeManager : public QObject {
    Q_OBJECT

private slots:
    void testColorCalculation();
    void testThemeSwitching();
    void testThemePersistence();
};
```

### 集成测试

**测试重点：**
- 划词翻译的端到端流程
- 主题切换的视觉效果
- 动画性能测试
- 跨平台兼容性测试

### 手动测试

**测试场景：**
1. **主题测试**: 在不同系统上验证深色主题效果
2. **交互测试**: 验证输入框任意位置点击功能
3. **划词测试**: 在不同应用中测试划词翻译功能
4. **性能测试**: 长时间使用的性能表现

---

## 📋 部署策略

### 版本兼容性

**兼容性要求：**
- 支持现有配置文件格式
- 保持现有 API 接口不变
- 支持旧版本的迁移

**迁移策略：**
1. **配置迁移**: 自动升级配置文件
2. **功能开关**: 提供新旧功能切换选项
3. **向后兼容**: 保持现有功能的可用性

### 发布计划

**分阶段发布：**
1. **第一阶段**: 深色主题 + 输入框交互优化
2. **第二阶段**: 划词翻译功能
3. **第三阶段**: 界面精美化和动画效果

### 回滚策略

**风险管理：**
- 每个功能模块可独立启用/禁用
- 提供配置选项回滚到旧版本
- 保持代码的模块化设计，便于问题定位

---

## 🔒 安全考虑

### 数据安全

**敏感信息保护：**
- API 密钥继续使用现有加密存储
- 用户配置文件不包含敏感信息
- 翻译内容不在本地持久化

### 权限管理

**权限最小化：**
- 只请求必要的系统权限
- 跨平台文本检测在用户授权后进行
- 热键注册遵循系统权限管理

### 错误处理

**容错机制：**
- 网络错误自动重试
- 跨平台 API 调用异常处理
- 资源不足时的优雅降级

---

## 📈 监控和分析

### 性能监控

**监控指标：**
- 启动时间
- 内存占用
- CPU 使用率
- 渲染帧率

### 用户行为分析

**分析重点：**
- 功能使用频率
- 用户操作路径
- 性能瓶颈识别
- 错误日志收集

---

## 🎯 成功标准

### 技术指标

**性能指标：**
- 启动时间 < 3 秒
- 内存占用 < 100MB
- 动画帧率 ≥ 60fps
- 热键响应时间 < 100ms

**功能指标：**
- 深色主题覆盖率 100%
- 输入框交互成功率 100%
- 划词翻译准确率 ≥ 95%
- 跨平台兼容性 100%

### 用户体验指标

**满意度指标：**
- 用户界面满意度 ≥ 4.5/5
- 功能易用性 ≥ 4.5/5
- 学习成本降低 ≥ 50%
- 操作效率提升 ≥ 30%

---

## 🚀 下一步行动

### 开发优先级

**第一优先级** (立即开始):
1. 实现主题管理器
2. 重构 GTextEdit 组件
3. 创建基础样式组件

**第二优先级** (2周内):
1. 实现文本选择检测
2. 开发划词翻译服务
3. 创建翻译弹窗

**第三优先级** (4周内):
1. 添加动画效果
2. 优化性能
3. 完善测试

### 风险控制

**高风险项目：**
- 跨平台文本选择检测
- 建议在 Windows 上先实现原型验证

**中等风险项目：**
- 性能优化
- 建议分阶段优化，持续监控

### 资源需求

**开发资源：**
- 前端开发 (QML): 2人周
- 后端开发 (C++): 3人周
- 测试资源: 1人周
- 设计资源: 0.5人周

**技术资源：**
- Windows、macOS、Linux 测试环境
- 性能分析工具
- 自动化测试框架
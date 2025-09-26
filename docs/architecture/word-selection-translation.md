# 划词翻译功能架构

## 跨平台文本选择检测

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

## 跨平台实现策略

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

## 划词翻译服务

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

## 翻译弹窗架构

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
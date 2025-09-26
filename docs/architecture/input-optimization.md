# 输入框交互优化架构

## GTextEdit 组件重构

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

## 事件处理流程

```
用户点击 → MouseArea 捕获 → 计算文本位置 → 设置焦点 → 移动光标 → 完成交互
```

## 兼容性保证

- 保持现有 API 接口不变
- 保持现有信号和槽机制
- 保持现有文本选择和编辑功能
- 保持滚动功能正常工作
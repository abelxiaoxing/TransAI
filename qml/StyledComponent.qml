import QtQuick 2.15
import QtQuick.Controls 2.15
import "."

// 基础样式组件，所有自定义组件都应该继承此组件
Rectangle {
    id: root

    // 主题颜色属性
    property color backgroundColor: ThemeColors.background
    property color borderColor: ThemeColors.border
    property color textColor: ThemeColors.foreground
    property color accentColor: ThemeColors.accent

    // 设计令牌
    property real borderRadius: ThemeColors.radius
    property real borderWidth: 1

    // 状态属性
    property bool hovered: false
    property bool pressed: false
    property bool enabled: true
    property bool focused: false

    // 透明度
    property real opacityEnabled: 1.0
    property real opacityDisabled: 0.5
    property real opacityHovered: 0.9
    property real opacityPressed: 0.8

    // 动画
    Behavior on color {
        ColorAnimation {
            duration: ThemeColors.animationDuration
            easing.type: Easing.OutQuad
        }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: ThemeColors.animationDuration
            easing.type: Easing.OutQuad
        }
    }

    // 应用主题样式
    color: backgroundColor
    border.color: borderColor
    border.width: borderWidth
    radius: borderRadius
    opacity: enabled ? opacityEnabled : opacityDisabled

    // 交互效果
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        propagateComposedEvents: true

        onEntered: {
            if (root.enabled) {
                root.hovered = true
                root.opacity = opacityHovered
            }
        }

        onExited: {
            if (root.enabled) {
                root.hovered = false
                root.opacity = opacityEnabled
            }
        }

        onPressed: {
            if (root.enabled) {
                root.pressed = true
                root.opacity = opacityPressed
            }
        }

        onReleased: {
            if (root.enabled) {
                root.pressed = false
                root.opacity = root.hovered ? opacityHovered : opacityEnabled
            }
        }
    }
}
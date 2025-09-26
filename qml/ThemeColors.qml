import QtQuick 2.15

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
    readonly property color error: "#F48771"
    readonly property color success: "#4EC9B0"
    readonly property color warning: "#FFD166"

    // 统一的设计令牌
    readonly property real radius: 8
    readonly property real radiusSmall: 4
    readonly property real radiusLarge: 12
    readonly property real spacing: 8
    readonly property real spacingSmall: 4
    readonly property real spacingLarge: 16
    readonly property real spacingXLarge: 24
    readonly property real animationDuration: 250
    readonly property real animationDurationFast: 150
    readonly property real animationDurationSlow: 350

    // 文字样式
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeNormal: 14
    readonly property int fontSizeLarge: 16
    readonly property int fontSizeXLarge: 18

    // WCAG AA 对比度验证函数
    function getContrastRatio(color1, color2) {
        // 简化的对比度计算，实际实现需要更精确的算法
        return Math.abs(getLuminance(color1) - getLuminance(color2));
    }

    function getLuminance(color) {
        var r = color.r * 255;
        var g = color.g * 255;
        var b = color.b * 255;

        // 相对亮度计算
        var lr = r <= 0.03928 ? r / 12.92 : Math.pow((r + 0.055) / 1.055, 2.4);
        var lg = g <= 0.03928 ? g / 12.92 : Math.pow((g + 0.055) / 1.055, 2.4);
        var lb = b <= 0.03928 ? b / 12.92 : Math.pow((b + 0.055) / 1.055, 2.4);

        return 0.2126 * lr + 0.7152 * lg + 0.0722 * lb;
    }

    // 验证 WCAG AA 标准 (对比度 >= 4.5)
    readonly property bool wcagCompliant: getContrastRatio(background, foreground) >= 4.5

    // 主题应用函数
    function applyTheme(component) {
        if (component && component.hasOwnProperty("color")) {
            component.color = foreground;
        }
        if (component && component.hasOwnProperty("backgroundColor")) {
            component.backgroundColor = background;
        }
    }
}
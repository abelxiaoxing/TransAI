# Hyprland 一键划词翻译

TransAI 在 Wayland/Hyprland 下通过 Primary Selection 读取鼠标选中的文本。正常情况下不需要按 `Ctrl+C`，也不会改写普通剪贴板。

## 依赖

安装 `wl-clipboard`，并确认下面的命令能够输出当前选中的文本：

```bash
wl-paste --primary --no-newline
```

## 推荐配置

先让 TransAI 随 Hyprland 在后台启动：

```ini
exec-once = TransAI --background
```

然后绑定一键划词翻译：

```ini
bind = SUPER, T, exec, TransAI --translate-selection
```

使用流程：

1. 在浏览器、编辑器或其他应用中用鼠标选中文本。
2. 按 `SUPER+T`。
3. TransAI 获取 Primary Selection，开始翻译并显示结果。

`--translate-selection` 不会重复启动 GUI。它会通过用户私有的本地 socket 将命令发送给已运行的 TransAI；如果 TransAI 尚未运行，则会在后台启动并执行这次翻译。

## 应用兼容性

Primary Selection 是否产生由文本所在应用决定。少数应用、Canvas 内容和图片不会提供 Primary Selection；这类内容后续应通过 AT-SPI 或 OCR provider 获取。取词失败时 TransAI 会显示明确原因，不会回退并误翻译普通剪贴板中的旧内容。

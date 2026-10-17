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

## 点击窗口外自动隐藏（可选权限）

Wayland 协议不会把落在其他窗口上的点击转发给 TransAI，焦点跟随鼠标（Hyprland 默认 `input:follow_mouse=1`）时“失焦”也无法区分“鼠标移开”与“点击别处”。因此 TransAI 的 `GlobalClickMonitor` 直接从 evdev 设备节点（`/dev/input/event*`）被动读取物理左键按下（只读、不 grab），配合窗口的指针悬停状态判断点击是否落在窗外。

这需要会话用户能读取指针设备节点。推荐用 udev uaccess 规则（只覆盖鼠标/触摸板等指针设备，不含键盘）：

```udev
# /etc/udev/rules.d/70-transai-pointing-uaccess.rules
ACTION=="add|change", SUBSYSTEM=="input", KERNEL=="event*", ENV{ID_INPUT_MOUSE}=="1", TAG+="uaccess"
ACTION=="add|change", SUBSYSTEM=="input", KERNEL=="event*", ENV{ID_INPUT_TOUCHPAD}=="1", TAG+="uaccess"
ACTION=="add|change", SUBSYSTEM=="input", KERNEL=="event*", ENV{ID_INPUT_POINTINGSTICK}=="1", TAG+="uaccess"
ACTION=="add|change", SUBSYSTEM=="input", KERNEL=="event*", ENV{ID_INPUT_TABLET}=="1", TAG+="uaccess"
```

```bash
sudo udevadm control --reload
sudo udevadm trigger --subsystem-match=input --action=add
```

注意规则文件名必须排在 `73-seat-late.rules` 之前（如 `70-` 前缀），否则 `uaccess` builtin 已执行完毕，ACL 不会生效。也可以用 `sudo usermod -aG input $USER` 代替（需重新登录，且同时暴露键盘设备）。

没有该权限时应用照常运行，只是在焦点跟随鼠标的 compositor 下退化为 Esc / ✕ 按钮关闭窗口；启动日志会打印一次提示。

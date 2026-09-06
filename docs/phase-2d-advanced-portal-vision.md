# Phase 2D 实施任务书：高级 Wayland Portal 与多模态图片翻译

> 目标读者：负责实现 Phase 2D 的 coding agent  
> 适用项目：TransAI / Qt6 / QML / C++17  
> 前置条件：建议已完成 Phase 2A / 2B，Phase 2C 可选

---

## 0. 执行前必读

实现前请先阅读：

- `AGENTS.md`
- `docs/phase-1-architecture-refactor.md`
- `docs/phase-2-wayland-selection-and-image-translation.md`
- `docs/phase-2a-selection-translation.md`
- `docs/phase-2b-screenshot-ocr-translation.md`
- `src/input/ShortcutBackend.h`
- `src/input/GlobalShortcutManager.h/cpp`
- `src/input/SelectionTextProvider.h/cpp`
- `src/app/TranslationCoordinator.h/cpp`
- `src/translation/TranslationService.h/cpp`
- `CMakeLists.txt`

本阶段属于高级能力，不应阻塞基础划词翻译和 OCR 翻译闭环。

---

## 1. 本阶段定位

Phase 2A 已解决基础闭环：

```text
Wayland: Primary Selection → Clipboard(manual copy) → Manual Copy Fallback → TranslationService
Non-Wayland: ClipboardGuard + simulate Ctrl/Cmd+C → TranslationService
```

Phase 2D 只做增强，不替代 Phase 2A fallback。

本阶段可拆成两个互相独立的方向：

```text
A. 高级 Wayland Portal 能力
   - PortalShortcutBackend
   - 可选、实验性的 RemoteDesktop Portal selection strategy

B. 多模态图片翻译能力
   - VisionTranslationService
   - ImagePayloadBuilder
   - OpenAI-compatible vision payload
```

如果上下文或工期有限，必须继续拆小任务实现。不要把 Portal、RemoteDesktop、Vision 一次性塞进一个提交。

---

## 2. 非目标

本阶段不要做：

- 不重写 Phase 2A 的基础 fallback。
- 不删除 Manual Copy fallback。
- 不让 Portal Shortcut 失败导致应用不可用。
- 不把 RemoteDesktop Portal 作为默认取词路径；它只能作为显式启用的实验能力。
- 不把多模态 payload 放进普通 `PromptBuilder`。
- 不让 `TranslationService` 同时处理文本和图片 payload。
- 不破坏 OCR then Translate 路径。
- 不恢复旧 `Controller` / 旧 `Hotkey` wrapper。

---

## 3. 官方行为约束

### 3.1 GlobalShortcuts Portal

Wayland 下真正的应用级全局快捷键应使用：

```text
xdg-desktop-portal GlobalShortcuts
```

接口：

```text
org.freedesktop.portal.Desktop
/org/freedesktop/portal/desktop
org.freedesktop.portal.GlobalShortcuts
```

主要流程：

```text
CreateSession
    ↓
BindShortcuts
    ↓
用户通过系统 Portal UI 确认或修改快捷键
    ↓
监听 Activated(session_handle, shortcut_id, timestamp, options)
    ↓
emit activated(actionId)
```

关键点：

- `preferred_trigger` 只是建议，不是应用强制绑定。
- 用户可以拒绝、修改或清空快捷键。
- 不同 compositor / portal backend 支持度不同。
- 能力不可用时必须 fallback 到 Phase 2A 的托盘菜单、应用内按钮、手动复制路径。

### 3.2 Shortcuts trigger 格式

Portal 使用 freedesktop shortcuts spec，不是 Qt `QKeySequence::toString()` 格式。

示例：

```text
Qt/UI:  Ctrl+Shift+T
Portal: CTRL+SHIFT+t

Qt/UI:  Ctrl+Alt+Return
Portal: CTRL+ALT+Return

Qt/UI:  Meta+Space
Portal: LOGO+space
```

常见映射：

```text
Ctrl  -> CTRL
Alt   -> ALT
Shift -> SHIFT
Meta  -> LOGO
A-Z   -> a-z
Enter -> Return
Return -> Return
```

转换应封装在 `PortalShortcutBackend` 内部，不要污染 QML。

---

## 4. PortalShortcutBackend

### 4.1 新增文件

```text
src/input/PortalShortcutBackend.h
src/input/PortalShortcutBackend.cpp
```

### 4.2 职责

实现 Phase 1 的 `ShortcutBackend` 接口，尝试使用：

```text
xdg-desktop-portal GlobalShortcuts
```

它只负责：

- 检测 portal 是否可用。
- 创建 GlobalShortcuts session。
- 绑定 shortcut id 与 preferred trigger。
- 监听 `Activated` 信号。
- 发出 `activated(actionId)` 或 `failed(actionId, reason)`。

它不负责：

- 读取剪贴板。
- 模拟 Ctrl+C。
- 获取选中文本。
- 调用 `TranslationService`。
- 操作 QML UI。
- 保存设置。

### 4.3 推荐接口

`PortalShortcutBackend` 继续实现：

```cpp
class PortalShortcutBackend : public ShortcutBackend
{
    Q_OBJECT

public:
    explicit PortalShortcutBackend(QObject* parent = nullptr);
    ~PortalShortcutBackend() override;

    bool registerShortcut(const QString& actionId, const QKeySequence& sequence) override;
    void unregisterShortcut(const QString& actionId) override;

private:
    QString toPortalTrigger(const QKeySequence& sequence) const;
};
```

### 4.4 实现阶段拆分

#### 4.4.1 Scaffold 阶段

第一步可只做 scaffold：

```text
- 类存在并实现 ShortcutBackend
- Wayland 下可被 GlobalShortcutManager 选择
- 如果 Qt DBus 或 Portal 不可用，明确 failed(actionId, reason)
- 不崩溃，不静默失败
```

该阶段如果不真正调用 D-Bus，则不要提前添加无用 DBus 依赖。

#### 4.4.2 完整 D-Bus 阶段

完整实现时再引入 Qt DBus：

```text
CreateSession(options) -> request handle
等待 org.freedesktop.portal.Request::Response
读取 session_handle
BindShortcuts(session_handle, shortcuts, parent_window, options) -> request handle
等待 Response，确认 shortcut 是否 bound
connect Activated signal
Activated shortcut_id -> emit activated(shortcut_id)
```

### 4.5 错误处理

以下情况必须 `failed(actionId, reason)`：

- 当前不是 Wayland 且被错误选择。
- Session bus 不可用。
- `org.freedesktop.portal.Desktop` 不存在。
- `org.freedesktop.portal.GlobalShortcuts` interface 不存在。
- portal version 不满足最低要求。
- `QKeySequence` 无法转换为 portal trigger。
- 用户拒绝绑定。
- 返回的 bound shortcuts 不包含请求的 action id。
- D-Bus 请求超时或返回错误。

不要静默失败。

### 4.6 降级顺序

推荐：

```text
1. PortalShortcutBackend（Wayland 且配置 auto/portal）
2. QHotkeyShortcutBackend（非 Wayland，或显式 xcb）
3. 应用内快捷键
4. 托盘菜单 action
5. 手动按钮
6. Manual Copy Fallback
```

注意：Wayland 下不应 fallback 到 XTest/QHotkey 伪装全局快捷键。

### 4.7 CMake

完整 D-Bus 实现时需要：

```cmake
find_package(Qt6 REQUIRED COMPONENTS Quick DBus)
```

并链接：

```cmake
target_link_libraries(TransAI PRIVATE Qt6::Quick Qt6::DBus ...)
```

新增：

```cmake
set(HEADERS
    ...
    src/input/PortalShortcutBackend.h
)

set(SOURCES
    ...
    src/input/PortalShortcutBackend.cpp
)
```

如果只是 scaffold，不调用 Qt DBus API，则不要提前添加 `Qt6::DBus`。

---

## 5. GlobalShortcutManager backend 选择

`GlobalShortcutManager` 应负责选择 backend，但不负责具体业务。

推荐逻辑：

```cpp
if (platformName contains "wayland") {
    // auto 或 portal 模式：PortalShortcutBackend
    // disabled 模式：不注册，发 failed 或返回 false
} else {
    // QHotkeyShortcutBackend
}
```

将来支持设置项：

```json
"wayland": {
  "shortcutBackend": "auto"
}
```

取值建议：

```text
auto      Wayland 下尝试 Portal，非 Wayland 下使用 QHotkey
portal    强制 Portal，仅 Wayland 有意义
qhotkey   非 Wayland 使用；Wayland 下应提示不可用
disabled  不注册全局快捷键，只保留托盘/应用按钮
```

---

## 6. Wayland 设置页语义

当前 X11 思路是：

```text
用户录制 Ctrl+Shift+T
    ↓
应用强制注册 Ctrl+Shift+T
```

Wayland Portal 语义应改为：

```text
应用提供 preferred_trigger
    ↓
系统弹出 Portal 配置 UI
    ↓
用户确认、修改或拒绝
```

因此设置页应区分：

### 6.1 非 Wayland

继续显示：

```text
Click to record
```

保存 `shortCut` / `shortcuts.translateSelection` 并调用 QHotkey 注册。

### 6.2 Wayland

显示：

```text
Configure System Shortcut
```

说明：

```text
Wayland 下全局快捷键由系统 Portal 授权和管理。
你可以在系统弹窗中确认或修改快捷键。
```

按钮触发：

```text
GlobalShortcutManager.registerShortcut("translateSelection", preferredShortcut)
```

如果后续支持 `ConfigureShortcuts`，可提供单独按钮重新打开系统配置 UI。

### 6.3 显示 bound trigger

Portal 返回 `trigger_description` 时，设置页应显示实际绑定结果，例如：

```text
System shortcut: Ctrl+Shift+T
```

不要只显示用户最初输入的 preferred shortcut。

---

## 7. RemoteDesktop Portal Selection Strategy

### 7.1 背景

Wayland 下无法通用读取其他应用选中文本。Phase 2A 已提供：

```text
Primary Selection
Clipboard
Manual Copy Fallback
```

本阶段可增加高级策略：

```text
RemoteDesktop Portal + Ctrl+C
```

该策略属于实验能力，不应作为 Phase 2 的核心验收项，也不应影响基础 fallback。

### 7.2 默认状态

必须默认关闭：

```json
"wayland": {
  "useRemoteDesktopPortal": false,
  "selectionMode": "auto"
}
```

只有用户明确启用后才尝试。UI 文案必须标注为“实验性 / 需要系统授权 / 可能在当前桌面环境不可用”。

### 7.3 推荐流程

```text
用户启用高级选项
    ↓
请求 RemoteDesktop portal 权限
    ↓
通过 portal 发送 Ctrl+C
    ↓
等待剪贴板变化
    ↓
ClipboardGuard 读取并尽量恢复剪贴板
    ↓
selectedTextReady(text)
```

### 7.4 要求

- 默认关闭。
- 标记为实验能力，不纳入基础功能必过路径。
- 权限拒绝时有明确提示。
- 失败时 fallback 到 Clipboard / Manual Copy。
- 不作为唯一选中文本路径。
- 不破坏 Phase 2A 的 Primary Selection / Clipboard fallback。
- 不能在 `PortalShortcutBackend` 中实现取词或模拟输入。

---

## 8. VisionTranslationService

### 8.1 新增文件

```text
src/vision/VisionTranslationService.h
src/vision/VisionTranslationService.cpp
src/vision/ImagePayloadBuilder.h
src/vision/ImagePayloadBuilder.cpp
```

### 8.2 职责

支持：

```text
图片 → 多模态 LLM → 翻译结果
```

而不是：

```text
图片 → OCR → 文本翻译
```

OCR then Translate 仍由 Phase 2B 路径负责。

### 8.3 推荐接口

```cpp
class VisionTranslationService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString responseData READ responseData NOTIFY responseDataChanged)
    Q_PROPERTY(QString responseError READ responseError NOTIFY responseErrorChanged)
    Q_PROPERTY(bool isRequesting READ isRequesting NOTIFY isRequestingChanged)

public:
    explicit VisionTranslationService(QObject* parent = nullptr);

    Q_INVOKABLE void translateImageFromFile(const QString& filePath, const QVariantMap& options);
    Q_INVOKABLE void translateImage(const QImage& image, const QVariantMap& options);
    Q_INVOKABLE void abort();

signals:
    void responseDataChanged();
    void responseErrorChanged();
    void isRequestingChanged();
    void translationStarted();
    void translationFinished(QString result);
    void translationFailed(QString reason);
};
```

### 8.4 注意

`VisionTranslationService` 不应该复用普通 `PromptBuilder` 构造图片 payload。

原因：

- OpenAI-compatible vision chat 通常使用 `content` 数组。
- Ollama vision 可能有不同字段。
- 图片需要压缩、编码、MIME 处理。
- 文本翻译 prompt 和图片 payload 应解耦。

---

## 9. ImagePayloadBuilder

### 9.1 职责

```text
QImage / file path
    ↓
校验文件和图片格式
    ↓
缩放 / 压缩 / 转换格式
    ↓
Base64 data URL 或 provider-specific payload
```

### 9.2 推荐配置

```json
"imageTranslation": {
  "mode": "ocrThenTranslate",
  "visionModel": "",
  "maxImageWidth": 1600,
  "jpegQuality": 85
}
```

### 9.3 要求

- 控制图片最大宽度。
- 控制 JPEG 质量。
- 支持 data URL。
- 错误时返回清晰原因。
- 不要在 QML 中手写 base64 payload。
- 不要污染普通文本 `PromptBuilder`。

---

## 10. Provider 策略

第一版建议只支持：

```text
OpenAI-compatible vision chat
```

不要一次性支持所有 Provider。

要求：

- Provider 不支持 vision 时给出明确错误。
- Ollama vision 支持可以后续单独扩展。
- 不要影响普通 OpenAI/Ollama 文本翻译。
- `visionModel` 为空时不能误走 vision。

---

## 11. TranslationCoordinator 接入

如果启用多模态图片翻译：

```text
translateImageFile(filePath)
    ↓
根据 imageTranslation.mode 判断
    ├─ ocrThenTranslate -> Phase 2B 路径
    └─ visionModel -> VisionTranslationService
```

要求：

- 默认模式仍为 `ocrThenTranslate`。
- `visionModel` 只有配置启用且模型非空时才走。
- Vision 失败时可以提示用户切换 OCR 模式。
- Vision 路径不能影响普通文本翻译。

---

## 12. 设置项

建议：

```json
"wayland": {
  "shortcutBackend": "auto",
  "selectionMode": "auto",
  "useRemoteDesktopPortal": false,
  "useScreenshotPortal": true
},
"imageTranslation": {
  "mode": "ocrThenTranslate",
  "visionModel": "",
  "maxImageWidth": 1600,
  "jpegQuality": 85
}
```

注意：

- `useRemoteDesktopPortal` 默认必须是 `false`。
- `imageTranslation.mode` 默认必须是 `ocrThenTranslate`。
- `visionModel` 为空时不能误走 vision。
- 旧字段必须继续兼容：`shortCut`、`apiKey`、`apiServer`、`model`、`provider` 等。

---

## 13. 验收清单

### 13.1 Portal Shortcut 验收

- [ ] `PortalShortcutBackend` 实现 `ShortcutBackend` 接口。
- [ ] Portal 不可用时发出 failed。
- [ ] Portal 不可用时 fallback 可用。
- [ ] `preferred_trigger` 转换符合 freedesktop shortcuts spec。
- [ ] 用户拒绝授权时不崩溃。
- [ ] Activated 能正确转为 `activated(actionId)`。
- [ ] 不读取剪贴板。
- [ ] 不调用翻译服务。
- [ ] 不模拟 Ctrl+C。
- [ ] 不崩溃、不静默失败。

### 13.2 Wayland 设置页验收

- [ ] Wayland 下不承诺强制绑定用户输入的快捷键。
- [ ] 显示“由系统 Portal 配置快捷键”的说明。
- [ ] 可触发系统快捷键配置流程。
- [ ] 能显示实际 bound trigger 或清晰 unknown 状态。
- [ ] 非 Wayland 下原有录制快捷键行为不退化。

### 13.3 RemoteDesktop Portal 验收

- [ ] 默认关闭。
- [ ] 用户启用后才尝试。
- [ ] 权限拒绝时有提示。
- [ ] 失败时 fallback 到 Clipboard / Manual Copy。
- [ ] 尽量恢复剪贴板。
- [ ] 不作为唯一选中文本路径。

### 13.4 VisionTranslationService 验收

- [ ] `VisionTranslationService` 独立于 `TranslationService`。
- [ ] `ImagePayloadBuilder` 负责图片压缩和 base64。
- [ ] 第一版至少支持 OpenAI-compatible vision，或提供 scaffold 和清晰未实现错误。
- [ ] Provider 不支持时有明确错误。
- [ ] `ocrThenTranslate` 默认路径不受影响。
- [ ] 普通文本翻译不退化。

### 13.5 构建验收

至少执行：

```bash
cmake --build build -j$(nproc)
```

---

## 14. 推荐提交拆分

### Portal Shortcut

```text
feat(input): add portal shortcut backend scaffold
feat(input): convert Qt shortcuts to portal triggers
feat(input): support xdg desktop portal global shortcuts
feat(input): integrate portal shortcut fallback chain
feat(settings): adapt shortcut settings for Wayland portal
```

### RemoteDesktop Selection（实验能力，可后置到 Phase 3）

```text
feat(input): add optional remote desktop selection strategy
feat(input): guard remote desktop selection behind settings
feat(input): fallback remote desktop failures to clipboard
```

### Vision Translation

```text
feat(vision): add image payload builder
feat(vision): add vision translation service scaffold
feat(vision): support OpenAI-compatible vision requests
feat(app): route image translation mode through coordinator
```

---

## 15. 注意事项

1. 本阶段高风险，建议继续拆小任务实现。
2. Portal 相关能力不可用时必须 fallback。
3. RemoteDesktop Portal 不能默认启用，且应明确标注为实验能力。
4. Portal Shortcut 失败不能影响托盘菜单、应用内按钮、手动复制翻译。
5. Vision 模式不能影响 OCR then Translate。
6. 不要把图片 payload 塞进普通 `PromptBuilder`。
7. 不要让多模态 Provider 差异污染文本翻译服务。
8. 不要恢复旧 `Controller` / 旧 `Hotkey` wrapper。

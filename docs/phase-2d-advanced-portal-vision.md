# Phase 2D 实施任务书：高级 Wayland Portal 与多模态图片翻译

> 目标读者：负责实现 Phase 2D 的 coding agent  
> 适用项目：TransAI / Qt6 / QML / C++17  
> 前置条件：建议已完成 Phase 2A / 2B，Phase 2C 可选

---

## 0. 执行前必读

实现前请先阅读：

- `AGENTS.md`
- `docs/phase-1-architecture-refactor.md`
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

## 1. 本阶段目标

实现或预留高级能力：

```text
PortalShortcutBackend
RemoteDesktop Portal selection strategy
VisionTranslationService
ImagePayloadBuilder
OpenAI-compatible vision payload
```

本阶段可以拆成两个独立方向：

```text
A. 高级 Wayland Portal 能力
B. 多模态图片翻译能力
```

如果上下文或工期有限，建议继续拆成更小任务。

---

## 2. 非目标

本阶段不要做：

- 不重写 Phase 2A 的基础 fallback。
- 不删除 Manual Copy fallback。
- 不让 Portal Shortcut 失败导致应用不可用。
- 不把多模态 payload 放进普通 `PromptBuilder`。
- 不让 `TranslationService` 同时处理文本和图片 payload。
- 不破坏 OCR then Translate 路径。

---

## 3. PortalShortcutBackend

### 3.1 新增文件

```text
src/input/PortalShortcutBackend.h
src/input/PortalShortcutBackend.cpp
```

### 3.2 职责

实现 Phase 1 的 `ShortcutBackend` 接口，尝试使用：

```text
xdg-desktop-portal GlobalShortcuts
```

### 3.3 要求

- 能力不可用时发出：

```cpp
failed(actionId, reason)
```

- 不要崩溃。
- 不要静默失败。
- 不要在这里读取剪贴板。
- 不要在这里调用翻译服务。
- 不要在这里模拟 Ctrl+C。

### 3.4 降级顺序

```text
1. PortalShortcutBackend
2. QHotkeyShortcutBackend, only if not Wayland or explicitly xcb
3. 应用内快捷键
4. 托盘菜单 action
5. 手动按钮
```

### 3.5 CMake

如果实现 D-Bus Portal，需要：

```cmake
find_package(Qt6 REQUIRED COMPONENTS Quick DBus)
```

并链接：

```cmake
target_link_libraries(TransAI PRIVATE Qt6::Quick Qt6::DBus ...)
```

如果只是 scaffold，不要添加无用依赖。

---

## 4. RemoteDesktop Portal Selection Strategy

### 4.1 背景

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

### 4.2 推荐流程

```text
用户启用高级选项
    ↓
请求 RemoteDesktop portal 权限
    ↓
通过 portal 发送 Ctrl+C
    ↓
等待剪贴板变化
    ↓
ClipboardGuard 读取并尝试恢复剪贴板
    ↓
selectedTextReady(text)
```

### 4.3 要求

- 默认关闭。
- 由设置项控制：

```json
"wayland": {
  "useRemoteDesktopPortal": false,
  "selectionMode": "auto"
}
```

- 失败时 fallback 到 Clipboard / Manual Copy。
- 权限被拒绝时给出明确提示。
- 不要把该策略作为唯一可用路径。

---

## 5. VisionTranslationService

### 5.1 新增文件

```text
src/vision/VisionTranslationService.h
src/vision/VisionTranslationService.cpp
src/vision/ImagePayloadBuilder.h
src/vision/ImagePayloadBuilder.cpp
```

### 5.2 职责

支持：

```text
图片 → 多模态 LLM → 翻译结果
```

而不是：

```text
图片 → OCR → 文本翻译
```

### 5.3 推荐接口

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
    void translationFinished(QString result);
    void translationFailed(QString reason);
};
```

### 5.4 注意

`VisionTranslationService` 不应该复用普通 `PromptBuilder` 构造图片 payload。

原因：

- OpenAI-compatible vision chat 通常使用 `content` 数组。
- Ollama vision 可能有不同字段。
- 图片需要压缩、编码、MIME 处理。
- 文本翻译 prompt 和图片 payload 应解耦。

---

## 6. ImagePayloadBuilder

### 6.1 职责

```text
QImage / file path
    ↓
压缩 / 转换格式
    ↓
Base64 data URL 或 provider-specific payload
```

### 6.2 推荐配置

```json
"imageTranslation": {
  "mode": "ocrThenTranslate",
  "visionModel": "gpt-4o-mini",
  "maxImageWidth": 1600,
  "jpegQuality": 85
}
```

### 6.3 要求

- 控制图片最大宽度。
- 控制 JPEG 质量。
- 支持 data URL。
- 错误时返回清晰原因。
- 不要在 QML 中手写 base64 payload。

---

## 7. Provider 策略

第一版建议只支持：

```text
OpenAI-compatible vision chat
```

不要一次性支持所有 Provider。

要求：

- Provider 不支持 vision 时给出明确错误。
- Ollama vision 支持可以后续单独扩展。
- 不要影响普通 OpenAI/Ollama 文本翻译。

---

## 8. TranslationCoordinator 接入

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
- `visionModel` 只有配置启用时才走。
- Vision 失败时可以提示用户切换 OCR 模式。

---

## 9. 设置项

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

---

## 10. 验收清单

### 10.1 Portal Shortcut 验收

- [ ] `PortalShortcutBackend` 实现 `ShortcutBackend` 接口。
- [ ] Portal 不可用时发出 failed。
- [ ] Portal 不可用时 fallback 可用。
- [ ] 不读取剪贴板。
- [ ] 不调用翻译服务。
- [ ] 不崩溃。

### 10.2 RemoteDesktop Portal 验收

- [ ] 默认关闭。
- [ ] 用户启用后才尝试。
- [ ] 权限拒绝时有提示。
- [ ] 失败时 fallback 到 Clipboard / Manual Copy。
- [ ] 尽量恢复剪贴板。
- [ ] 不作为唯一选中文本路径。

### 10.3 VisionTranslationService 验收

- [ ] `VisionTranslationService` 独立于 `TranslationService`。
- [ ] `ImagePayloadBuilder` 负责图片压缩和 base64。
- [ ] 第一版至少支持 OpenAI-compatible vision，或提供 scaffold 和清晰未实现错误。
- [ ] Provider 不支持时有明确错误。
- [ ] `ocrThenTranslate` 默认路径不受影响。
- [ ] 普通文本翻译不退化。

### 10.4 构建验收

至少执行：

```bash
cmake --build build -j$(nproc)
```

---

## 11. 推荐提交拆分

### Portal Shortcut

```text
feat(input): add portal shortcut backend scaffold
feat(input): integrate portal shortcut fallback chain
feat(settings): expose shortcut backend selection
```

### RemoteDesktop Selection

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

## 12. 注意事项

1. 本阶段高风险，建议继续拆小任务实现。
2. Portal 相关能力不可用时必须 fallback。
3. RemoteDesktop Portal 不能默认启用。
4. Vision 模式不能影响 OCR then Translate。
5. 不要把图片 payload 塞进普通 `PromptBuilder`。
6. 不要让多模态 Provider 差异污染文本翻译服务。

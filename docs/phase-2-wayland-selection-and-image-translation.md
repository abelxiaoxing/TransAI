# Phase 2 总览：Wayland 划词翻译与图片翻译能力

> 本文是 Phase 2 的总览与导航文档。  
> **不要把本文作为单次实现任务书直接交给 agent 全量实现。**  
> 实际开发时请按子阶段分别把对应文档交给 agent。

---

## 0. 前置条件

开始 Phase 2 前必须先完成：

- `docs/phase-1-architecture-refactor.md`

Phase 2 依赖 Phase 1 中的核心模块：

```text
TranslationRequest
TranslationService
GlobalShortcutManager
SelectionTextProvider
ClipboardGuard
ProviderEndpointBuilder
StreamParser
```

如果这些模块尚未完成，不要直接进入 Phase 2。

---

## 1. 为什么 Phase 2 要拆成多份文档

Phase 2 涉及多个高风险能力：

- Wayland 选中文本获取
- 全局快捷键 fallback
- 截图 Portal
- OCR 外部进程
- Popup 展示
- 设置项扩展
- Portal Shortcut
- RemoteDesktop Portal
- 多模态图片翻译

这些能力不应该在一个上下文里一次性实现。原因：

1. 每个子任务依赖不同模块，失败点不同。
2. Wayland 行为强依赖桌面环境，需要分阶段验证。
3. OCR、Portal、Vision Provider 都有额外依赖，容易扩大改动范围。
4. 新开上下文交给 agent 时，单份任务书越聚焦，执行质量越高。

因此 Phase 2 拆成以下四份独立任务书。

---

## 2. 子阶段文档

### Phase 2A：划词翻译闭环

文档：

- `docs/phase-2a-selection-translation.md`

目标：

```text
快捷键 / 托盘菜单 / 应用内按钮
    ↓
SelectionTextProvider
    ↓
Primary Selection / Clipboard / Manual Copy Fallback
    ↓
TranslationCoordinator
    ↓
TranslationService
    ↓
主窗口展示
```

特点：

- 优先完成可用闭环。
- 不强依赖完整 Wayland GlobalShortcuts Portal。
- 不做 OCR。
- 不做截图。
- 不做多模态。
- 不强制实现 Popup。

建议第一个实现。

---

### Phase 2B：截图 OCR 翻译闭环

文档：

- `docs/phase-2b-screenshot-ocr-translation.md`

目标：

```text
截图 / 本地图片
    ↓
OcrService / ExternalProcessOcrService
    ↓
TranslationCoordinator
    ↓
TranslationService
    ↓
主窗口展示
```

特点：

- 使用 `ExternalProcessOcrService` 调用外部 `tesseract`。
- Wayland 下优先设计 Portal Screenshot，但允许先用本地图片输入完成 OCR 闭环。
- 不做多模态图片翻译。
- 不要求复杂设置页。

建议在 Phase 2A 稳定后实现。

---

### Phase 2C：Popup、设置页与体验增强

文档：

- `docs/phase-2c-ui-settings-polish.md`

目标：

```text
TranslationPopup
WaylandCapabilityStatus
ScreenshotPreview
设置页新增 Shortcut / Wayland / OCR 配置
```

特点：

- 只做体验层增强。
- 不改变底层翻译管线。
- 不实现新的 Portal 能力。
- 不实现多模态。

建议在 Phase 2A / 2B 闭环稳定后实现。

---

### Phase 2D：高级 Wayland Portal 与多模态图片翻译

文档：

- `docs/phase-2d-advanced-portal-vision.md`

目标：

```text
PortalShortcutBackend
RemoteDesktop Portal selection strategy
VisionTranslationService
ImagePayloadBuilder
OpenAI-compatible vision payload
```

特点：

- 高风险、高复杂度。
- 依赖不同桌面环境和 Provider 能力。
- 不应阻塞 Phase 2A / 2B 的核心闭环。

建议最后实现，或拆成后续 Phase 3。

---

## 3. 推荐执行顺序

```text
1. docs/phase-2a-selection-translation.md
2. docs/phase-2b-screenshot-ocr-translation.md
3. docs/phase-2c-ui-settings-polish.md
4. docs/phase-2d-advanced-portal-vision.md
```

如果资源有限，至少完成：

```text
Phase 2A：划词翻译闭环
Phase 2B：截图 / 图片 OCR 翻译闭环
```

`Phase 2C` 和 `Phase 2D` 可以后置。

---

## 4. 全局架构原则

所有子阶段都必须遵守：

```text
GlobalShortcutManager 只负责快捷键。
SelectionTextProvider 只负责获取文本。
ScreenshotCaptureService 只负责获取图片。
OcrService 只负责图片到文本。
VisionTranslationService 只负责图片多模态调用。
TranslationCoordinator 负责编排流程。
TranslationService 继续只负责文本翻译。
QML 负责展示，不负责编排复杂业务。
```

不要出现以下反向耦合：

```text
Hotkey 直接调用 TranslationService
SelectionTextProvider 直接操作 UI
TranslationService 处理截图 / OCR
PromptBuilder 构造图片 payload
QML 编排完整业务流程
```

---

## 5. 总体验收目标

Phase 2 全部完成后应支持：

```text
手动输入文本
    ↓
TranslationCoordinator / AppView
    ↓
TranslationService

选中文本
    ↓
GlobalShortcutManager / 托盘菜单 / 应用按钮
    ↓
SelectionTextProvider
    ↓
TranslationCoordinator
    ↓
TranslationService

截图或图片
    ↓
ScreenshotCaptureService / ImageInputService
    ↓
OcrService
    ↓
TranslationCoordinator
    ↓
TranslationService

后续多模态图片
    ↓
ImageInputService / ScreenshotCaptureService
    ↓
VisionTranslationService
```

并且：

- 手动翻译不退化。
- Wayland 能力不可用时不崩溃。
- 有清晰 fallback。
- OCR 依赖缺失时有清晰提示。
- 多模态功能不污染普通文本翻译路径。

---

## 6. 给 agent 的使用方式

新开上下文时建议只提供对应子阶段文档，例如：

```text
请根据 docs/phase-2a-selection-translation.md 实现 Phase 2A。
不要实现 Phase 2B/2C/2D。
```

或：

```text
请根据 docs/phase-2b-screenshot-ocr-translation.md 实现截图 OCR 翻译闭环。
假设 Phase 2A 已完成。
不要实现 Popup 或多模态图片翻译。
```

这样能降低误实现和过度实现风险。

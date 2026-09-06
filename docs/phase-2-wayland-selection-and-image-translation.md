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

## 1. Phase 2 总原则

Wayland 下不能默认假设：

```text
1. 应用可以私自注册全局快捷键
2. 应用可以模拟 Ctrl+C
3. 应用可以直接读取其他应用选中文本
4. 应用可以直接抓取屏幕内容
```

因此 Phase 2 必须采用：

```text
基础闭环优先
    ↓
明确 fallback
    ↓
高级 Portal 能力后置
```

不要把“Wayland 全局快捷键”和“Wayland 自动取词”混成一个问题：

```text
全局快捷键：后续用 xdg-desktop-portal GlobalShortcuts
选中文本获取：Wayland 下 Primary Selection / Clipboard(用户手动 Ctrl+C 后) / Manual Copy Fallback；非 Wayland 下模拟 Ctrl/Cmd+C 获取当前选区
自动复制：RemoteDesktop Portal 等高级能力，默认关闭，后置实现
```

---

## 2. 为什么 Phase 2 要拆成多份文档

Phase 2 涉及多个高风险能力：

- Wayland 选中文本获取
- 全局快捷键 fallback
- xdg-desktop-portal GlobalShortcuts
- 截图 Portal
- OCR 外部进程
- Popup 展示
- 设置项扩展
- RemoteDesktop Portal / 模拟输入
- 多模态图片翻译

这些能力不应该在一个上下文里一次性实现。原因：

1. 每个子任务依赖不同模块，失败点不同。
2. Wayland 行为强依赖桌面环境，需要分阶段验证。
3. OCR、Portal、Vision Provider 都有额外依赖，容易扩大改动范围。
4. 新开上下文交给 agent 时，单份任务书越聚焦，执行质量越高。

---

## 3. 子阶段文档

### Phase 2A：划词翻译基础闭环

文档：

- `docs/phase-2a-selection-translation.md`

目标：

```text
快捷键 / 托盘菜单 / 应用内按钮
    ↓
TranslationCoordinator.translateSelectedText()
    ↓
SelectionTextProvider
    ↓
Wayland: Primary Selection / Clipboard(manual copy) / Manual Copy Fallback
Non-Wayland: simulate Ctrl/Cmd+C with ClipboardGuard
    ↓
TranslationService
    ↓
主窗口展示
```

特点：

- 优先完成可用闭环。
- 不强依赖 Wayland GlobalShortcuts Portal。
- 必须提供不依赖全局快捷键且不依赖托盘的应用内触发入口；托盘菜单只能作为补充。
- Wayland 下失败时给出手动 Ctrl+C 提示。
- 不做 OCR。
- 不做截图。
- 不做多模态。
- 不强制实现 Popup。

建议第一个实现。

Phase 2A 推荐拆分：

```text
2A-1 修复 SelectionTextProvider fallback
2A-2 增加托盘菜单 / 应用内按钮触发入口
2A-3 引入 TranslationCoordinator
2A-4 改善快捷键失败提示
```

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
- 可为 Phase 2D 的 Portal Shortcut 设置页语义预留 UI。

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
- 不替代 Phase 2A 的 Manual Copy fallback。

建议最后实现，或拆成后续 Phase 3。

Phase 2D 推荐继续拆分：

```text
2D-1 PortalShortcutBackend scaffold
2D-2 PortalShortcutBackend 完整 D-Bus 实现
2D-3 Wayland 设置页适配 Portal 语义
2D-4 可选 RemoteDesktop Portal 取词策略
2D-5 VisionTranslationService / ImagePayloadBuilder
```

---

## 4. 推荐执行顺序

```text
1. docs/phase-2a-selection-translation.md
2. docs/phase-2b-screenshot-ocr-translation.md
3. docs/phase-2c-ui-settings-polish.md
4. docs/phase-2d-advanced-portal-vision.md
```

如果资源有限，至少完成：

```text
Phase 2A：划词翻译基础闭环
Phase 2B：截图 / 图片 OCR 翻译闭环
```

`Phase 2C` 和 `Phase 2D` 可以后置。

---

## 5. 全局架构原则

所有子阶段都必须遵守：

```text
GlobalShortcutManager 只负责快捷键。
ShortcutBackend 只负责注册/注销/激活信号。
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
ShortcutBackend 读取剪贴板
PortalShortcutBackend 模拟 Ctrl+C
SelectionTextProvider 直接操作 UI
TranslationService 处理截图 / OCR
PromptBuilder 构造图片 payload
QML 编排完整业务流程
```

---

## 6. Wayland 快捷键策略

### 6.1 Phase 2A

Phase 2A 不依赖完整 Portal 快捷键。

```text
Wayland 下全局快捷键注册失败
    ↓
显示清晰提示
    ↓
使用托盘菜单 / 应用内按钮 / 手动 Ctrl+C fallback
```

### 6.2 Phase 2D

真正的 Wayland 全局快捷键在 Phase 2D 实现：

```text
PortalShortcutBackend
    ↓
xdg-desktop-portal GlobalShortcuts
    ↓
CreateSession
    ↓
BindShortcuts(preferred_trigger)
    ↓
Activated
```

注意：

```text
preferred_trigger 只是建议，不是应用强制绑定。
用户可以通过系统 UI 修改或拒绝。
```

---

## 7. 总体验收目标

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
AppView.startTrans("screenshotOcr") / TranslationService

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
- 全局快捷键不可用时至少有应用内按钮；托盘菜单作为补充入口。
- 选中文本不可读时有手动 Ctrl+C 提示。
- OCR 依赖缺失时有清晰提示。
- 多模态功能不污染普通文本翻译路径。

---

## 8. 给 agent 的使用方式

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

或：

```text
请根据 docs/phase-2d-advanced-portal-vision.md 只实现 PortalShortcutBackend scaffold。
不要实现 VisionTranslationService，不要实现 RemoteDesktop Portal。
```

这样能降低误实现和过度实现风险。

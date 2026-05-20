# Phase 1 架构重构状态：Controller / Hotkey 旧源码清理版

> 适用项目：TransAI / Qt6 / QML / C++17  
> 当前结论：Phase 1 运行时已迁移到新模块路径，旧 `src/controller.*` 与 `src/hotkey.*` 必须删除，不再保留源码备查或兼容入口。

## 1. 本阶段硬性边界

当前代码仍处于开发阶段，不需要兼容旧 C++ 类型或旧源码路径。

必须满足：

1. 不保留 `Controller` 类。
2. 不保留 `Hotkey` 类。
3. 不保留 `src/controller.h` / `src/controller.cpp`。
4. 不保留 `src/hotkey.h` / `src/hotkey.cpp`。
5. `CMakeLists.txt` 不再引用旧 Controller / Hotkey 源码。
6. QML 不再创建或引用旧 `Hotkey` 类型。
7. 翻译入口唯一为 `TranslationService`。
8. 模型检测入口唯一为 `ModelDiscoveryService`。
9. 全局快捷键入口为 `GlobalShortcutManager`。
10. 选中文本获取入口为 `SelectionTextProvider`。

`QHotkeyShortcutBackend` 是新输入架构的 backend，允许继续使用第三方 QHotkey 库；禁止恢复旧 `Hotkey` wrapper。

## 2. 当前目标目录结构

```text
src/
├── main.cpp
├── stdafx.h
├── updater.h/.cpp
├── settings/
│   ├── Setting.h
│   └── Setting.cpp
├── translation/
│   ├── TranslationRequest.h/.cpp
│   ├── TranslationService.h/.cpp
│   ├── PromptBuilder.h/.cpp
│   └── StreamParser.h/.cpp
├── providers/
│   ├── ProviderEndpointBuilder.h/.cpp
│   └── ModelDiscoveryService.h/.cpp
└── input/
    ├── ShortcutBackend.h
    ├── GlobalShortcutManager.h/.cpp
    ├── QHotkeyShortcutBackend.h/.cpp
    ├── SelectionTextProvider.h/.cpp
    └── ClipboardGuard.h/.cpp
```

旧路径清理后，配置能力由 `settings/Setting.*` 承担；翻译、模型检测、快捷键、选中文本获取均不经过 Controller / Hotkey。

## 3. 运行时调用方向

### 3.1 手动翻译

```text
AppView.qml
  ↓ translateFromMap(...)
TranslationService
  ↓
PromptBuilder
ProviderEndpointBuilder
StreamParser
```

要求：

- `AppView.qml` 直接创建或使用 `TranslationService`。
- `Stop` 按钮直接调用 `translationService.abort()`。
- Busy 状态直接绑定 `translationService.isRequesting`。
- 流式结果直接来自 `translationService.responseData`。
- 错误展示直接来自 `translationService.responseError`。
- 不允许 `AppView.qml -> APIController -> TranslationService` 代理路径。

### 3.2 模型检测

```text
SettingView.qml
  ↓ detectModels(...)
ModelDiscoveryService
  ↓
ProviderEndpointBuilder
```

要求：

- `SettingView.qml` 直接使用 `ModelDiscoveryService`。
- 不允许通过 `Controller::detectModels()` 或 `APIController.detectModels()` 代理。
- Ollama model list 必须使用用户配置的 `apiServer`，归一化到 `/api/tags`。

### 3.3 划词快捷键

```text
main.qml / SettingView.qml
  ↓
GlobalShortcutManager
  ↓ activated("translateSelection")
SelectionTextProvider
  ↓ selectedTextReady(text)
AppView.startTrans()
```

要求：

- 快捷键注册只在 `GlobalShortcutManager` / `ShortcutBackend` 中处理。
- 模拟复制、剪贴板 marker、剪贴板恢复只在 `SelectionTextProvider` / `ClipboardGuard` 中处理。
- `QHotkeyShortcutBackend` 不读取剪贴板、不模拟复制、不知道翻译。
- `SelectionTextProvider` 不知道 UI 和翻译服务。
- Wayland 下允许快捷键或模拟复制明确失败，但不得崩溃或误用旧 Hotkey。

## 4. 模块职责

### 4.1 Setting

位置：`src/settings/Setting.h`、`src/settings/Setting.cpp`

职责：

- 读取和保存 `config.json`。
- 保留历史配置字段兼容读取：`apiKey`、`apiServer`、`model`。
- 管理 provider-specific 配置：OpenAI / Ollama server、key、model。
- 暴露当前 active provider 的 `apiServer`、`apiKey`、`model` 给 QML。

不负责：翻译请求、模型检测、快捷键、主题、选中文本获取。

### 4.2 TranslationRequest

职责：

- 从 `QVariantMap` 生成结构化翻译请求。
- 支持 mode 字符串与旧 int 映射：
  - `0 -> Translate`
  - `1 -> ExplainWord`
  - `2 -> CorrectGrammar`
- 支持 source 预留：`ManualInput`、`SelectedText`、`ScreenshotOcr`、`ImageMultimodal`。

### 4.3 PromptBuilder

职责：只根据 `TranslationRequest` 构造 LLM messages。

要求：

- 三种 mode 的 prompt 与旧行为保持一致。
- 网络请求类中不得再拼 prompt。

### 4.4 StreamParser

职责：解析 OpenAI-compatible SSE streaming response。

必须支持：

- `data: {...}`。
- `data: [DONE]`。
- error object。
- 单 chunk 多个 SSE event。
- 一个 JSON 被拆成多个 chunk。
- content 中包含前导或尾随空白字符时不被错误裁剪。

### 4.5 ProviderEndpointBuilder

职责：根据 provider 与 apiServer 构造 endpoint。

要求：

- OpenAI 默认 server：`https://api.openai.com`。
- Ollama 默认 server：`http://localhost:11434`。
- OpenAI chat completion 归一化到 `/v1/chat/completions` 或兼容路径。
- OpenAI model list 归一化到 `/v1/models`。
- Ollama 翻译请求使用 OpenAI-compatible `/v1/chat/completions`。
- Ollama model list 使用 `/api/tags`，包括用户输入 `/v1`、`/api/chat`、`/v1/chat/completions` 等情况。
- 不得硬编码忽略用户配置的 Ollama server。

### 4.6 ModelDiscoveryService

职责：唯一模型检测入口。

暴露属性：

- `availableModels`
- `isDetecting`
- `error`

要求：

- 新检测开始前取消旧请求。
- 提前失败必须重置 `isDetecting`。
- OpenAI 解析 `data[].id`。
- Ollama 解析 `models[].name`。

### 4.7 TranslationService

职责：唯一文本翻译入口。

暴露属性：

- `responseData`
- `responseError`
- `isRequesting`

要求：

- 新请求开始前取消并清理旧 reply，避免旧信号污染新请求。
- 所有提前失败路径必须重置 `isRequesting`。
- 提前失败应清空上一轮 responseData，避免 UI 显示过期结果。
- 支持 `abort()`。
- 不负责 UI、快捷键、选中文本获取、截图、OCR、设置界面。

### 4.8 Input 模块

- `ShortcutBackend`：快捷键 backend 抽象。
- `QHotkeyShortcutBackend`：基于 QHotkey 的非 Wayland backend，只负责注册/注销/激活信号。
- `GlobalShortcutManager`：QML 面向的快捷键管理入口。
- `ClipboardGuard`：保存并尽量恢复剪贴板文本。
- `SelectionTextProvider`：设置 marker、模拟 Ctrl/Cmd+C、读取剪贴板、恢复剪贴板、发出成功/失败信号。

## 5. CMake 要求

`CMakeLists.txt` 必须包含新模块，不得包含旧源码：

```cmake
set(HEADERS
    src/stdafx.h
    src/updater.h
    src/settings/Setting.h
    src/translation/TranslationRequest.h
    src/translation/TranslationService.h
    src/translation/PromptBuilder.h
    src/translation/StreamParser.h
    src/providers/ProviderEndpointBuilder.h
    src/providers/ModelDiscoveryService.h
    src/input/ShortcutBackend.h
    src/input/GlobalShortcutManager.h
    src/input/QHotkeyShortcutBackend.h
    src/input/SelectionTextProvider.h
    src/input/ClipboardGuard.h
)

set(SOURCES
    src/main.cpp
    src/updater.cpp
    src/settings/Setting.cpp
    src/translation/TranslationRequest.cpp
    src/translation/TranslationService.cpp
    src/translation/PromptBuilder.cpp
    src/translation/StreamParser.cpp
    src/providers/ProviderEndpointBuilder.cpp
    src/providers/ModelDiscoveryService.cpp
    src/input/GlobalShortcutManager.cpp
    src/input/QHotkeyShortcutBackend.cpp
    src/input/SelectionTextProvider.cpp
    src/input/ClipboardGuard.cpp
)
```

## 6. QML 要求

### 6.1 AppView.qml

必须直接使用：

```qml
TranslationService {
    id: translationService
}
```

请求结构示例：

```qml
translationService.translateFromMap({
    "text": inputArea.text,
    "targetLanguage": langSelector.currentText,
    "mode": 0,
    "source": "manual",
    "provider": setting.provider,
    "apiServer": setting.apiServer,
    "apiKey": setting.apiKey,
    "model": setting.model
})
```

### 6.2 SettingView.qml

必须直接使用：

```qml
ModelDiscoveryService {
    id: modelDetector
}
```

快捷键设置页不得引用旧 `Hotkey`。如需统一注册函数，应通过 `main.qml` 提供的 `shortcutRegistrar` 或等价新对象调用 `GlobalShortcutManager`。

### 6.3 main.qml

必须使用：

```qml
GlobalShortcutManager { ... }
SelectionTextProvider { ... }
```

不允许：

```qml
Hotkey { ... }
APIController { ... }
```

## 7. Review 重点

每次改动后执行：

```bash
rg -n "src/controller|src/hotkey|class Controller|class Hotkey|APIController|Controller::sendMessage|Controller::detectModels" src qml CMakeLists.txt
```

预期：无命中。`QHotkeyShortcutBackend` 和第三方 QHotkey 库命中是允许的。

再执行：

```bash
cmake --build build -j$(nproc)
```

## 8. 验收清单

### 架构验收

- [x] `src/controller.h` / `src/controller.cpp` 已删除。
- [x] `src/hotkey.h` / `src/hotkey.cpp` 已删除。
- [x] `CMakeLists.txt` 不再引用旧 Controller / Hotkey 源码。
- [x] 配置能力迁移到 `src/settings/Setting.*`。
- [x] Prompt 构造已从 Controller 拆出。
- [x] Stream 解析已从 Controller 拆出且支持 SSE 拆包。
- [x] Endpoint 构造已从 Controller 拆出。
- [x] Model Detect 由 `ModelDiscoveryService` 承担，QML 不经过 Controller。
- [x] `TranslationService` 是唯一翻译入口。
- [x] QML 通过 `GlobalShortcutManager` + `SelectionTextProvider` 使用新快捷键路径。
- [x] 未提前加入截图 / OCR / 多模态逻辑。

### 功能验收

- [ ] 手动输入翻译正常。
- [ ] Streaming 输出正常。
- [ ] Stop / Abort 正常。
- [ ] OpenAI Provider 正常。
- [ ] Ollama Provider 正常。
- [ ] Ollama 使用用户配置的 apiServer。
- [ ] Model Detect 正常。
- [ ] 设置保存正常。
- [ ] Provider 切换正常。
- [ ] 旧配置字段兼容读取正常。
- [ ] 快捷键配置不丢失。

### 构建验收

- [x] `cmake --build build -j$(nproc)` 通过。

## 9. 禁止回退项

后续 Phase 1 修复中禁止重新引入：

- `src/controller.h`
- `src/controller.cpp`
- `src/hotkey.h`
- `src/hotkey.cpp`
- `Controller::sendMessage()`
- `Controller::detectModels()`
- QML `Hotkey {}`
- QML `APIController {}`
- `AppView.qml -> APIController -> TranslationService` 代理路径
- `SettingView.qml -> APIController -> ModelDiscoveryService` 代理路径

如需要新增主题或配置能力，应创建职责明确的新模块，而不是恢复旧 Controller 巨型类。

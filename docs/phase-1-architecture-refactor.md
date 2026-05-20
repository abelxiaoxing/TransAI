# Phase 1 实施任务书：TransAI 架构稳定化重构（分批低风险版）

> 目标读者：负责实现 Phase 1 的 coding agent  
> 适用项目：TransAI / Qt6 / QML / C++17  
> 当前阶段目标：**稳定拆分现有架构，不实现截图翻译，不实现完整 Wayland Portal，不一次性重写 QML**

---

## 0. 执行前必读

实现前请先阅读项目根目录的：

- `AGENTS.md`
- `src/controller.h`
- `src/controller.cpp`
- `src/hotkey.h`
- `src/hotkey.cpp`
- `src/main.cpp`
- `qml/main.qml`
- `qml/AppView.qml`
- `qml/SettingView.qml`
- `CMakeLists.txt`

实现过程中应优先保证：

1. 当前用户可见功能不退化。
2. 每一步都能单独构建验证。
3. 先抽取 C++ 内部逻辑，再逐步迁移 QML。
4. 保留旧 `Controller` / `Hotkey` 作为兼容门面，直到新路径稳定。

---

## 1. 背景

当前 TransAI 的核心逻辑集中在少量类中：

```text
src/controller.h/cpp
 ├─ Setting 配置管理
 ├─ ThemeController
 ├─ Controller 翻译请求
 ├─ Prompt 构造
 ├─ OpenAI/Ollama Endpoint 构造
 ├─ Streaming Response 解析
 ├─ Model Detect
 └─ 错误处理

src/hotkey.h/cpp
 ├─ 全局快捷键注册
 ├─ 模拟 Ctrl+C / Cmd+C
 ├─ 读取剪贴板
 └─ 暴露 selectedText

qml/main.qml
 ├─ 创建 Hotkey
 ├─ 监听 selectedTextChanged
 └─ 直接调用 appView.startTrans()
```

这种结构可以支持当前手动翻译，但不利于长期维护，也不利于后续加入：

- Wayland 环境下的划词快捷键翻译
- 截图翻译
- OCR 或多模态图片翻译
- 多输入来源
- 多展示方式
- 多 Provider 扩展

Phase 1 的任务是先把架构拆开，让后续 Phase 2 能安全落地。

---

## 2. Phase 1 总目标

完成后系统应满足：

1. 当前手动输入翻译功能不退化。
2. 当前 Provider 设置、Model Detect、Ollama/OpenAI 翻译不退化。
3. `Controller` 不再继续承载所有新增职责。
4. 翻译核心逻辑逐步迁移到 `TranslationService`。
5. Prompt 构造、Stream 解析、Endpoint 构造、Model Detect 从 `Controller` 中抽出。
6. 快捷键注册和选中文本获取从现有 `Hotkey` 中拆分，但旧 `Hotkey` 可保留兼容。
7. 为 Wayland 后续实现预留抽象接口，但 Phase 1 不要求实现完整 Portal 能力。

---

## 3. 非目标

Phase 1 **不要做**以下内容：

- 不实现截图翻译。
- 不实现 OCR。
- 不实现多模态图片翻译。
- 不重写整个 QML UI。
- 不重做主题系统。
- 不删除旧配置兼容逻辑。
- 不强行实现完整 Wayland GlobalShortcuts Portal。
- 不把截图、OCR、复杂弹窗逻辑塞进本阶段。
- 不一次性删除 `Controller` 或 `Hotkey`。

---

## 4. 推荐分批策略

本阶段必须拆成两个子阶段执行，避免一次性大改 C++ 和 QML。

### 4.1 Phase 1A：内部抽取，暂不大改 QML

目标：

```text
AppView.qml / SettingView.qml
    ↓ 仍可使用 APIController
Controller 兼容门面
    ↓
PromptBuilder / StreamParser / ProviderEndpointBuilder / ModelDiscoveryService
```

要求：

- `APIController` 仍然可用于旧 QML。
- `Controller::sendMessage()` 可以先内部调用新模块。
- `Controller::detectModels()` 可以代理到 `ModelDiscoveryService`。
- 用户可见行为不变化。

### 4.2 Phase 1B：引入 TranslationService，并逐步迁移 QML

目标：

```text
AppView.qml
    ↓
TranslationService
    ↓
PromptBuilder / ProviderEndpointBuilder / StreamParser
```

要求：

- `TranslationService` 成为新的翻译入口。
- `AppView.qml` 可从 `APIController` 迁移到 `TranslationService`。
- `Controller` 仍可作为兼容层保留。
- 不要把 `ThemeController` 迁移问题混入本阶段主线。

---

## 5. 目标目录结构

建议新增以下目录和文件：

```text
src/
├── translation/
│   ├── TranslationRequest.h
│   ├── TranslationRequest.cpp
│   ├── TranslationService.h
│   ├── TranslationService.cpp
│   ├── PromptBuilder.h
│   ├── PromptBuilder.cpp
│   ├── StreamParser.h
│   └── StreamParser.cpp
│
├── providers/
│   ├── ProviderEndpointBuilder.h
│   ├── ProviderEndpointBuilder.cpp
│   ├── ModelDiscoveryService.h
│   └── ModelDiscoveryService.cpp
│
└── input/
    ├── ShortcutBackend.h
    ├── GlobalShortcutManager.h
    ├── GlobalShortcutManager.cpp
    ├── QHotkeyShortcutBackend.h
    ├── QHotkeyShortcutBackend.cpp
    ├── SelectionTextProvider.h
    ├── SelectionTextProvider.cpp
    ├── ClipboardGuard.h
    └── ClipboardGuard.cpp
```

`src/controller.h/cpp` 和 `src/hotkey.h/cpp` 可以保留作为兼容层。新代码应逐步迁移到上述模块。

---

## 6. 设计原则

实现过程中必须遵守以下原则：

```text
不要让 Hotkey 知道翻译。
不要让 SelectionTextProvider 知道 UI。
不要让 TranslationService 知道截图或 OCR。
不要让 QML 编排复杂业务流程。
不要让 Controller 继续成为新增功能的万能类。
不要破坏现有配置文件兼容性。
不要为了重构而同时改动主题系统。
```

推荐最终调用方向：

```text
QML / Coordinator
    ↓
TranslationRequest
    ↓
TranslationService
    ↓
PromptBuilder
    ↓
ProviderEndpointBuilder
    ↓
Network Request
    ↓
StreamParser
    ↓
QML 展示结果
```

---

## 7. 任务一：抽取 TranslationRequest

### 7.1 新增文件

```text
src/translation/TranslationRequest.h
src/translation/TranslationRequest.cpp
```

### 7.2 推荐内容

```cpp
#ifndef TRANSLATION_REQUEST_H
#define TRANSLATION_REQUEST_H

#include <QString>
#include <QVariantMap>

enum class TranslationSource {
    ManualInput,
    SelectedText,
    ScreenshotOcr,
    ImageMultimodal
};

enum class TranslationMode {
    Translate,
    ExplainWord,
    CorrectGrammar
};

struct TranslationRequest {
    QString text;
    QString targetLanguage;
    TranslationMode mode = TranslationMode::Translate;
    TranslationSource source = TranslationSource::ManualInput;

    QString provider;
    QString apiServer;
    QString apiKey;
    QString model;
};

TranslationMode translationModeFromString(const QString& value);
TranslationMode translationModeFromInt(int value);
TranslationSource translationSourceFromString(const QString& value);
TranslationRequest translationRequestFromMap(const QVariantMap& map);

#endif // TRANSLATION_REQUEST_H
```

### 7.3 要求

- QML 侧主要通过 `QVariantMap` 调用。
- 保留手动翻译 `ManualInput`。
- 预留 `SelectedText`、`ScreenshotOcr`、`ImageMultimodal`，但 Phase 1 不实现这些完整功能。
- `mode` 不要再长期依赖裸 `int`。
- 为兼容旧逻辑，必须支持：

```text
0 -> Translate
1 -> ExplainWord
2 -> CorrectGrammar
```

---

## 8. 任务二：抽取 PromptBuilder

### 8.1 新增文件

```text
src/translation/PromptBuilder.h
src/translation/PromptBuilder.cpp
```

### 8.2 职责

`PromptBuilder` 只负责根据 `TranslationRequest` 构造 LLM messages。

从 `Controller::sendMessage(QString str, int mode)` 中迁出以下逻辑：

```cpp
if (mode == 0) {
    // Translate
} else if (mode == 1) {
    // Explain word
} else {
    // Correct grammar
}
```

### 8.3 推荐接口

```cpp
class PromptBuilder
{
public:
    QJsonArray buildMessages(const TranslationRequest& request) const;

private:
    QJsonObject createMessage(const QString& role, const QString& content) const;
};
```

### 8.4 验收要求

- 原有三种模式输出逻辑保持一致。
- Prompt 构造代码不再出现在网络请求类中。
- 后续添加新 mode 时，只需要改 `PromptBuilder`。
- 建议为三种 mode 增加轻量单元测试或可执行验证用例。

---

## 9. 任务三：抽取 StreamParser（必须支持拆包）

### 9.1 新增文件

```text
src/translation/StreamParser.h
src/translation/StreamParser.cpp
```

### 9.2 职责

解析 OpenAI-compatible streaming response。

从 `Controller` 中迁出：

```cpp
_getContent()
_parseResponse()
_getError()
```

### 9.3 推荐接口

不要使用完全无状态的 `parseChunk(const QByteArray&) const` 作为最终设计。真实 SSE 可能出现半包、多个 `data:` frame、`[DONE]` 等情况，因此建议使用有状态 parser：

```cpp
struct StreamParseResult {
    QString content;
    QString errorMessage;
    bool hasError = false;
    bool done = false;
};

class StreamParser
{
public:
    void reset();
    StreamParseResult appendChunk(const QByteArray& data);

private:
    QByteArray m_buffer;
    StreamParseResult parseEventPayload(const QByteArray& payload) const;
};
```

### 9.4 要求

- 能解析现有 OpenAI-compatible SSE：`data: {...}`。
- 能解析 `data: [DONE]`。
- 能解析 error object。
- 能处理一个 chunk 内多个 `data:`。
- 能处理一个 JSON 被拆成多个 chunk 的情况。
- 不要依赖 UI。
- 不要依赖 `Controller`。

### 9.5 建议测试样例

至少覆盖：

```text
data: {"choices":[{"delta":{"content":"hello"}}]}

data: {"choices":[{"delta":{"content":" world"}}]}

data: [DONE]
```

以及拆包：

```text
data: {"choices":[{"delta"
```

下一块：

```text
:{"content":"hello"}}]}
```

---

## 10. 任务四：抽取 ProviderEndpointBuilder

### 10.1 新增文件

```text
src/providers/ProviderEndpointBuilder.h
src/providers/ProviderEndpointBuilder.cpp
```

### 10.2 职责

从 provider 和 apiServer 生成：

- Chat Completion URL
- Model List URL

从 `Controller` 中迁出：

```cpp
_buildModelListUrl()
_buildChatCompletionUrl()
```

### 10.3 推荐接口

```cpp
class ProviderEndpointBuilder
{
public:
    QUrl buildChatCompletionUrl(const QString& apiServer, const QString& provider) const;
    QUrl buildModelListUrl(const QString& apiServer, const QString& provider) const;
};
```

### 10.4 要求

- OpenAI 默认 API Server 为 `https://api.openai.com`。
- Ollama 默认 API Server 为 `http://localhost:11434`。
- 保持当前路径兼容逻辑，例如：
  - `/v1`
  - `/models`
  - `/chat/completions`
  - `/api/tags`
- 必须修复当前 Ollama model list URL 硬编码问题：

```cpp
// 不要忽略用户配置的 apiServer
QUrl url(apiServer.trimmed().isEmpty()
    ? "http://localhost:11434"
    : apiServer.trimmed());
```

---

## 11. 任务五：抽取 ModelDiscoveryService

### 11.1 新增文件

```text
src/providers/ModelDiscoveryService.h
src/providers/ModelDiscoveryService.cpp
```

### 11.2 职责

替代 `Controller::detectModels()` 的核心实现，独立负责：

- 检测模型
- 解析模型列表
- 暴露状态
- 暴露错误信息

### 11.3 推荐接口

```cpp
class ModelDiscoveryService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList availableModels READ availableModels NOTIFY availableModelsChanged)
    Q_PROPERTY(bool isDetecting READ isDetecting NOTIFY isDetectingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit ModelDiscoveryService(QObject* parent = nullptr);

    QStringList availableModels() const;
    bool isDetecting() const;
    QString error() const;

    Q_INVOKABLE void detectModels(QString apiServer, QString apiKey, QString provider);

signals:
    void availableModelsChanged();
    void isDetectingChanged();
    void errorChanged();

private:
    QStringList parseModelList(const QByteArray& data, const QString& provider, QString* errorMessage) const;
};
```

### 11.4 兼容策略

第一步可以保留 QML：

```qml
APIController {
    id: modelDetector
}
```

然后让 `Controller::detectModels()` 代理到 `ModelDiscoveryService`。

待稳定后，再改为：

```qml
ModelDiscoveryService {
    id: modelDetector
}
```

---

## 12. 任务六：新增 TranslationService

### 12.1 新增文件

```text
src/translation/TranslationService.h
src/translation/TranslationService.cpp
```

### 12.2 职责

统一文本翻译入口。

`TranslationService` 负责：

- 接收 `TranslationRequest`
- 构造 request body
- 发起网络请求
- 处理 streaming result
- 暴露 `responseData` / `responseError` / `isRequesting`
- 支持 `abort`

`TranslationService` 不负责：

- UI 展示
- 快捷键
- 选中文本获取
- 截图
- OCR
- 设置界面
- 主题系统

### 12.3 推荐接口

```cpp
class TranslationService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString responseData READ responseData NOTIFY responseDataChanged)
    Q_PROPERTY(QString responseError READ responseError NOTIFY responseErrorChanged)
    Q_PROPERTY(bool isRequesting READ isRequesting NOTIFY isRequestingChanged)

public:
    explicit TranslationService(QObject* parent = nullptr);

    QString responseData() const;
    QString responseError() const;
    bool isRequesting() const;

    Q_INVOKABLE void translateFromMap(const QVariantMap& requestMap);
    Q_INVOKABLE void abort();

signals:
    void responseDataChanged();
    void responseErrorChanged();
    void isRequestingChanged();

    void translationStarted();
    void translationFinished(QString result);
    void translationFailed(QString error);

private:
    void translate(const TranslationRequest& request);
};
```

### 12.4 兼容要求

当前 `AppView.qml` 依赖：

```qml
api.responseData
api.responseError
api.isRequesting
api.abort()
api.sendMessage(...)
```

Phase 1 推荐先这样做：

```text
AppView.qml -> APIController -> TranslationService
```

待验证稳定后，再迁移到：

```qml
translationService.responseData
translationService.responseError
translationService.isRequesting
translationService.abort()
translationService.translateFromMap(...)
```

---

## 13. 任务七：QML 接入 TranslationService

### 13.1 注册方式

在 `src/main.cpp` 中注册：

```cpp
qmlRegisterType<TranslationService>("TransAI", 1, 0, "TranslationService");
qmlRegisterType<ModelDiscoveryService>("TransAI", 1, 0, "ModelDiscoveryService");
```

也可以通过 context property 注入单例对象，但要避免多个 QML 页面各自创建不必要的重复服务。

### 13.2 `AppView.qml` 修改目标

从：

```qml
APIController {
    id: api
}
```

逐步迁移到：

```qml
TranslationService {
    id: translationService
}
```

点击翻译：

```qml
translationService.translateFromMap({
    "text": inputArea.text,
    "targetLanguage": langSelector.currentText,
    "mode": "translate",
    "source": "manual",
    "provider": setting.provider,
    "apiServer": setting.apiServer,
    "apiKey": setting.apiKey,
    "model": setting.model
})
```

### 13.3 注意事项

- `Stop` 按钮继续工作。
- BusyIndicator 继续工作。
- 流式输出继续实时更新。
- 语言选择继续影响目标语言。
- Provider 切换后继续使用最新 setting。
- 不要在同一步里重构主题系统。

---

## 14. 任务八：拆分 Hotkey（Phase 1 后半段）

### 14.1 当前问题

当前 `Hotkey` 同时负责：

```text
快捷键注册
模拟复制
读取剪贴板
暴露 selectedText
```

Phase 1 后半段需要拆为：

```text
GlobalShortcutManager
ShortcutBackend
QHotkeyShortcutBackend
SelectionTextProvider
ClipboardGuard
```

---

## 15. 新增 ShortcutBackend

### 15.1 新增文件

```text
src/input/ShortcutBackend.h
```

### 15.2 推荐接口

```cpp
class ShortcutBackend : public QObject
{
    Q_OBJECT

public:
    explicit ShortcutBackend(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ShortcutBackend() = default;

    virtual bool registerShortcut(const QString& actionId, const QKeySequence& sequence) = 0;
    virtual void unregisterShortcut(const QString& actionId) = 0;

signals:
    void activated(QString actionId);
    void failed(QString actionId, QString reason);
};
```

---

## 16. 新增 QHotkeyShortcutBackend

### 16.1 新增文件

```text
src/input/QHotkeyShortcutBackend.h
src/input/QHotkeyShortcutBackend.cpp
```

### 16.2 职责

用当前 `QHotkey` 逻辑实现非 Wayland 后端。

适用：

```text
Windows
macOS
X11
```

### 16.3 要求

- 不在这里读取剪贴板。
- 不在这里模拟 Ctrl+C。
- 只发出 `activated(actionId)`。
- Wayland 下应明确返回失败或不启用。

---

## 17. 新增 GlobalShortcutManager

### 17.1 新增文件

```text
src/input/GlobalShortcutManager.h
src/input/GlobalShortcutManager.cpp
```

### 17.2 推荐接口

```cpp
class GlobalShortcutManager : public QObject
{
    Q_OBJECT

public:
    explicit GlobalShortcutManager(QObject* parent = nullptr);

    Q_INVOKABLE bool registerShortcut(const QString& actionId, const QString& sequence);
    Q_INVOKABLE void unregisterShortcut(const QString& actionId);

signals:
    void shortcutActivated(QString actionId);
    void shortcutRegisterFailed(QString actionId, QString reason);
};
```

### 17.3 Phase 1 行为

支持至少一个 action：

```text
translateSelection
```

但 Phase 1 不要求完整实现划词翻译闭环，只要完成抽象拆分并保持现有快捷键能力可用。

---

## 18. 新增 ClipboardGuard

### 18.1 新增文件

```text
src/input/ClipboardGuard.h
src/input/ClipboardGuard.cpp
```

### 18.2 职责

保护用户剪贴板，供 `SelectionTextProvider` 使用。

### 18.3 推荐行为

```text
保存当前 clipboard text
执行复制动作
读取新 clipboard text
尽量恢复旧 clipboard text
```

### 18.4 注意

Wayland 下剪贴板恢复可能不稳定，所以接口必须允许失败，不要崩溃。

---

## 19. 新增 SelectionTextProvider

### 19.1 新增文件

```text
src/input/SelectionTextProvider.h
src/input/SelectionTextProvider.cpp
```

### 19.2 推荐接口

```cpp
class SelectionTextProvider : public QObject
{
    Q_OBJECT

public:
    explicit SelectionTextProvider(QObject* parent = nullptr);

    Q_INVOKABLE void requestSelectedText();

signals:
    void selectedTextReady(QString text);
    void selectedTextFailed(QString reason);
};
```

### 19.3 Phase 1 行为

可迁移现有逻辑：

```text
模拟 Ctrl+C / Cmd+C
等待剪贴板变化
读取文本
```

但要注意：

- 该逻辑不应该留在 ShortcutBackend。
- 该逻辑不应该直接调用 TranslationService。
- 该逻辑应能被 Phase 2 替换为 Wayland strategies。

---

## 20. Controller / Hotkey 兼容策略

### 20.1 `Controller` 兼容

短期内不要删除 `Controller`，因为它还承担：

- 旧 QML API 入口
- `ThemeController` 暴露
- 旧属性绑定兼容

推荐：

```text
Controller::sendMessage()
    ↓
TranslationService::translateFromMap()
```

```text
Controller::detectModels()
    ↓
ModelDiscoveryService::detectModels()
```

也可以在第一步只复用 `PromptBuilder` / `StreamParser` / `ProviderEndpointBuilder`，待稳定后再代理到完整服务。

### 20.2 `Hotkey` 兼容

短期可保留旧 `Hotkey`，但新逻辑应迁移到：

```text
GlobalShortcutManager
SelectionTextProvider
```

旧 `Hotkey` 可在后续变成薄兼容层，或等 QML 完成迁移后再删除。

---

## 21. CMake 调整

新增 `.h/.cpp` 后必须更新 `CMakeLists.txt`。

示例：

```cmake
set(HEADERS
    ...
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
    ...
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

---

## 22. 推荐提交拆分

请尽量按逻辑拆 commit：

```text
refactor(translation): add translation request model
refactor(translation): extract prompt builder
refactor(translation): extract stateful stream parser
refactor(providers): extract endpoint builder
fix(providers): respect configured ollama api server
refactor(providers): extract model discovery service
feat(translation): add translation service
refactor(controller): proxy translation through new services
refactor(qml): route AppView through translation service
refactor(input): split global shortcut and selection provider
```

---

## 23. 验收清单

### 23.1 功能验收

- [ ] 手动输入翻译正常。
- [ ] Streaming 输出正常。
- [ ] Stop / Abort 正常。
- [ ] OpenAI Provider 正常。
- [ ] Ollama Provider 正常。
- [ ] Ollama 使用用户配置的 apiServer，不再被硬编码到 localhost。
- [ ] Model Detect 正常。
- [ ] 设置保存正常。
- [ ] Provider 切换正常。
- [ ] 旧配置文件兼容。
- [ ] 当前快捷键配置不丢失。

### 23.2 架构验收

- [ ] Prompt 构造已从 `Controller` 拆出。
- [ ] Stream 解析已从 `Controller` 拆出，且支持 SSE 拆包。
- [ ] Endpoint 构造已从 `Controller` 拆出。
- [ ] Model Detect 已由独立服务承担或由 `Controller` 代理。
- [ ] `TranslationService` 成为新的翻译入口，或已完成可验证的代理路径。
- [ ] `Controller` 不再承载新增翻译核心逻辑。
- [ ] 快捷键注册和选中文本获取已分离，或已进入 Phase 1 后半段迁移。
- [ ] 没有把 OCR / 截图逻辑提前塞进 Phase 1。

### 23.3 建议测试

建议新增或手动验证：

- [ ] `PromptBuilder` 三种 mode 输出与旧逻辑一致。
- [ ] `ProviderEndpointBuilder` 覆盖 OpenAI 与 Ollama 常见 URL。
- [ ] `StreamParser` 覆盖多个 `data:`、`[DONE]`、error object、拆包。

### 23.4 构建验收

至少执行：

```bash
cmake --build build -j$(nproc)
```

如果没有现成 build 目录，可按项目 `AGENTS.md` 中的方式配置构建。

---

## 24. 注意事项

1. 不要破坏当前 QML 属性绑定。
2. 不要改变用户配置文件路径。
3. 不要删除旧配置字段，例如 `apiKey`、`apiServer`、`model`。
4. 不要假设 Wayland 下全局快捷键一定可用。
5. 不要在 Phase 1 直接承诺 Wayland 划词翻译完整可用。
6. 所有新模块尽量保持 QObject 边界清晰，便于 QML 使用。
7. 不要把 `ThemeController` 迁移与翻译重构绑死在同一个提交里。

---

## 25. Phase 1 完成后的预期状态

Phase 1 完成后，代码应从：

```text
QML
 ↓
Controller 巨型类
 ↓
网络请求
```

演进为：

```text
QML
 ↓
TranslationService 或 Controller 兼容门面
 ↓
PromptBuilder / ProviderEndpointBuilder / StreamParser
 ↓
网络请求
```

快捷键相关代码应从：

```text
Hotkey = 快捷键 + 复制 + 剪贴板 + 选中文本
```

演进为：

```text
GlobalShortcutManager = 快捷键
SelectionTextProvider = 获取文本
ClipboardGuard = 保护剪贴板
```

这将为 Phase 2 的 Wayland 划词翻译、截图 OCR 翻译、多模态图片翻译打好基础。

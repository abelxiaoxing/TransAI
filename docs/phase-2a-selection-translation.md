# Phase 2A 实施任务书：Wayland 划词翻译闭环

> 目标读者：负责实现 Phase 2A 的 coding agent  
> 适用项目：TransAI / Qt6 / QML / C++17  
> 前置条件：已完成 `docs/phase-1-architecture-refactor.md`

---

## 0. 执行前必读

实现前请先阅读：

- `AGENTS.md`
- `docs/phase-1-architecture-refactor.md`
- `src/translation/TranslationRequest.h`
- `src/translation/TranslationService.h/cpp`
- `src/input/GlobalShortcutManager.h/cpp`
- `src/input/SelectionTextProvider.h/cpp`
- `src/input/ClipboardGuard.h/cpp`
- `qml/main.qml`
- `qml/AppView.qml`
- `qml/SettingView.qml`
- `CMakeLists.txt`

如果 Phase 1 尚未完成，请先实现 Phase 1，不要直接做本阶段。

---

## 1. 本阶段目标

实现一个可用、可降级的划词翻译闭环：

```text
快捷键 / 托盘菜单 / 应用内按钮
    ↓
TranslationCoordinator.translateSelectedText()
    ↓
SelectionTextProvider.requestSelectedText()
    ↓
Primary Selection / Clipboard / Manual Copy Fallback
    ↓
TranslationRequest(source = SelectedText)
    ↓
TranslationService
    ↓
主窗口展示结果
```

本阶段核心目标是 **闭环可用**，不是追求所有 Wayland 桌面环境下全自动划词。

---

## 2. 非目标

本阶段不要实现：

- 不实现截图翻译。
- 不实现 OCR。
- 不实现多模态图片翻译。
- 不实现完整 `PortalShortcutBackend`。
- 不实现 RemoteDesktop Portal + Ctrl+C。
- 不强制实现 `TranslationPopup.qml`。
- 不重写整个设置页。
- 不删除旧 `Hotkey` / `Controller` 兼容层。

---

## 3. Wayland 约束

Wayland 下不能默认假设：

- 应用可以监听全局快捷键。
- 应用可以模拟 Ctrl+C。
- 应用可以读取其他应用选中文本。
- `QClipboard::Selection` 一定可用。

因此本阶段必须采用 fallback 策略：

```text
1. 尝试读取 Primary Selection
2. 失败则读取普通 Clipboard
3. 仍失败则提示用户手动 Ctrl+C 后再触发翻译
```

不要承诺 Wayland 下全自动划词一定可用。

---

## 4. 新增 TranslationCoordinator

### 4.1 新增文件

```text
src/app/TranslationCoordinator.h
src/app/TranslationCoordinator.cpp
```

### 4.2 职责

`TranslationCoordinator` 负责应用级流程编排：

- 响应 `translateSelection` action。
- 调用 `SelectionTextProvider` 获取文本。
- 构造 `TranslationRequest`。
- 调用 `TranslationService`。
- 发出展示主窗口、提示、错误等信号。

它不负责：

- 具体快捷键注册。
- 具体剪贴板读取细节。
- 网络请求实现。
- UI 组件绘制。

### 4.3 推荐接口

```cpp
class TranslationCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit TranslationCoordinator(QObject* parent = nullptr);

    Q_INVOKABLE void translateSelectedText();

signals:
    void requestShowMainWindow();
    void requestSetInputText(QString text);
    void requestShowNotification(QString message);
    void requestShowError(QString message);
};
```

如果 Phase 1 已经预留了更完整接口，也可以保留：

```cpp
Q_INVOKABLE void translateManualText(QString text, QString targetLanguage);
Q_INVOKABLE void translateScreenshot();
Q_INVOKABLE void translateImageFile(QString filePath);
```

但本阶段只实现 `translateSelectedText()`。

---

## 5. Shortcut Action Routing

### 5.1 目标

支持 action：

```text
translateSelection
```

### 5.2 推荐流程

```text
GlobalShortcutManager.shortcutActivated("translateSelection")
    ↓
TranslationCoordinator.translateSelectedText()
```

### 5.3 集成方式

可以在 C++ 中连接：

```cpp
connect(shortcutManager, &GlobalShortcutManager::shortcutActivated,
        coordinator, [coordinator](const QString& actionId) {
    if (actionId == "translateSelection") {
        coordinator->translateSelectedText();
    }
});
```

也可以在 QML 中轻量转发，但不要把完整流程写进 QML。

### 5.4 降级触发方式

Wayland 下全局快捷键可能不可用，因此至少提供一种 fallback：

```text
1. GlobalShortcutManager 成功注册则使用快捷键
2. 注册失败时提供托盘菜单 action 或应用内按钮
3. 给出明确提示，不崩溃
```

如果当前项目托盘菜单接入成本较高，可以先提供应用内按钮触发，确保闭环可测。

---

## 6. SelectionTextProvider 策略

### 6.1 目标

让 `SelectionTextProvider` 支持多策略获取文本。

### 6.2 推荐优先级

```text
Strategy A: Primary Selection
Strategy B: Clipboard Text
Strategy C: Manual Copy Fallback
```

### 6.3 Strategy A：Primary Selection

尝试：

```cpp
QGuiApplication::clipboard()->text(QClipboard::Selection)
```

要求：

- 如果为空，继续 fallback。
- 不要假设所有 Wayland compositor 都支持。
- 非 Wayland 下也允许尝试，但不应造成错误。

### 6.4 Strategy B：Clipboard Text

读取普通剪贴板：

```cpp
QGuiApplication::clipboard()->text(QClipboard::Clipboard)
```

适合作为 fallback。用户可以手动复制后按快捷键。

### 6.5 Strategy C：Manual Copy Fallback

如果都失败，发出：

```cpp
selectedTextFailed("Wayland 当前环境无法直接读取选中文本，请先 Ctrl+C 复制文本，再使用翻译快捷键。")
```

或等价的本地化提示。

---

## 7. TranslationRequest 构造

获取文本成功后，coordinator 应构造：

```cpp
TranslationRequest {
    source = SelectedText
    mode = Translate
    text = selectedText
    targetLanguage = 当前设置或 UI 当前目标语言
    provider = setting.provider
    apiServer = setting.apiServer
    apiKey = setting.apiKey
    model = setting.model
}
```

如果当前架构中设置仍通过 QML/context property 提供，可以让 QML 将设置值传给 coordinator，或让 coordinator 持有可访问的设置对象。

不要在 `SelectionTextProvider` 中读取 provider、apiKey、model。

---

## 8. UI 展示方式

### 8.1 第一版使用主窗口展示

本阶段优先复用主窗口，不强制实现 Popup：

```text
获取 selected text
    ↓
填入 AppView inputArea 或通过 AppView 暴露的方法设置输入
    ↓
显示主窗口
    ↓
调用 TranslationService 翻译
```

### 8.2 QML 职责

QML 只做：

- 显示主窗口。
- 设置输入框文本。
- 展示翻译结果。
- 显示错误或通知。

QML 不应该直接编排：

```text
快捷键 -> 获取剪贴板 -> 构造 request -> 调用翻译
```

---

## 9. 设置兼容

必须保持旧字段兼容：

```text
shortCut
apiKey
apiServer
model
provider
openaiApiServer
openaiApiKey
openaiModel
ollamaApiServer
ollamaModel
```

本阶段可以暂不新增复杂配置结构。

如果新增快捷键配置，建议：

```json
"shortcuts": {
  "translateSelection": "Ctrl+Shift+T"
}
```

但旧 `shortCut` 必须继续可用。

---

## 10. CMake 调整

如新增文件，需要更新：

```text
CMakeLists.txt
```

示例：

```cmake
set(HEADERS
    ...
    src/app/TranslationCoordinator.h
)

set(SOURCES
    ...
    src/app/TranslationCoordinator.cpp
)
```

本阶段不应引入 Qt DBus，除非你明确实现了 Portal 相关能力。默认不需要。

---

## 11. 验收清单

### 11.1 功能验收

- [ ] 手动输入翻译不退化。
- [ ] `translateSelection` action 存在。
- [ ] 快捷键、托盘菜单或应用内按钮至少一种触发方式可用。
- [ ] 触发后进入 `TranslationCoordinator.translateSelectedText()`。
- [ ] 能尝试读取 Primary Selection。
- [ ] Primary Selection 为空时 fallback 到 Clipboard。
- [ ] Clipboard 为空时给出手动复制提示。
- [ ] 获取文本为空时不发起翻译。
- [ ] 获取文本成功后能通过 `TranslationService` 翻译。
- [ ] 结果能在主窗口展示。
- [ ] Wayland 下能力不可用时不崩溃。

### 11.2 架构验收

- [ ] `GlobalShortcutManager` 不读取剪贴板。
- [ ] `SelectionTextProvider` 不调用翻译服务。
- [ ] `TranslationService` 不知道快捷键来源。
- [ ] `TranslationCoordinator` 负责流程编排。
- [ ] QML 不承担复杂业务流程。
- [ ] 未实现截图 / OCR / 多模态。

### 11.3 构建验收

至少执行：

```bash
cmake --build build -j$(nproc)
```

如果没有现成 build 目录，可按项目 `AGENTS.md` 中的方式配置构建。

---

## 12. 推荐提交拆分

```text
feat(app): add translation coordinator
feat(input): route translate selection action
feat(input): add selection text fallback strategies
feat(qml): expose selection translation trigger
feat(translation): connect selected text flow to translation service
```

---

## 13. 注意事项

1. 不要把 RemoteDesktop Portal 放进本阶段。
2. 不要把截图翻译放进本阶段。
3. 不要为了划词翻译重写整个 `AppView.qml`。
4. 不要删除旧 `Hotkey`，除非已有完整替代并完成验证。
5. Wayland 下失败时要给出清晰提示，而不是静默失败。
6. 第一版以可用闭环为目标，Popup 和高级 Portal 后续再做。

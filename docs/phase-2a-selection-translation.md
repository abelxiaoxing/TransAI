# Phase 2A 实施任务书：Wayland 划词翻译基础闭环（优化版）

> 目标读者：负责实现 Phase 2A 的 coding agent  
> 适用项目：TransAI / Qt6 / QML / C++17  
> 前置条件：已完成 `docs/phase-1-architecture-refactor.md`

---

## 0. 执行前必读

实现前请先阅读：

- `AGENTS.md`
- `docs/phase-1-architecture-refactor.md`
- `docs/phase-2-wayland-selection-and-image-translation.md`
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

## 1. 本阶段核心结论

Wayland 下不能把以下能力当成默认可用：

```text
1. 应用私自注册全局快捷键
2. 应用模拟 Ctrl+C
3. 应用直接读取其他应用的选中文本
4. QClipboard::Selection 在所有 compositor 下稳定可用
```

因此 Phase 2A 的目标不是“一步实现 Wayland 下全自动划词快捷键翻译”，而是先完成**可用、可测、可降级**的基础闭环：

```text
快捷键 / 托盘菜单 / 应用内按钮
    ↓
TranslationCoordinator.translateSelectedText()
    ↓
SelectionTextProvider.requestSelectedText()
    ↓
Wayland: Primary Selection / Clipboard(用户手动 Ctrl+C 后) / Manual Copy Fallback
非 Wayland: ClipboardGuard + 模拟 Ctrl/Cmd+C / 明确失败提示
    ↓
TranslationRequest(source = SelectedText)
    ↓
TranslationService
    ↓
主窗口展示结果
```

后续高级能力（例如 `xdg-desktop-portal GlobalShortcuts`、RemoteDesktop Portal 模拟输入）放到 Phase 2D，不阻塞本阶段。

---

## 2. 本阶段目标

实现以下能力：

1. `SelectionTextProvider` 支持按平台区分的稳定 fallback：
   ```text
   Wayland: Primary Selection → Clipboard/manual copy → 提示先 Ctrl+C
   Non-Wayland: ClipboardGuard + 模拟 Ctrl/Cmd+C → 读取 Clipboard → 明确失败提示
   ```
2. 必须提供不依赖全局快捷键且不依赖托盘的触发入口，并推荐额外提供托盘入口：
   ```text
   必选：应用内按钮 Translate Selected / Translate Clipboard
   推荐：托盘菜单 Translate Selection
   ```
3. 引入最小 `TranslationCoordinator`，由它编排划词翻译流程。
4. 快捷键注册失败时给出清晰提示，不静默失败。
5. 保持手动输入翻译、Provider 设置、模型检测等 Phase 1 能力不退化。

---

## 3. 非目标

本阶段不要实现：

- 不实现 `PortalShortcutBackend` 的完整 D-Bus GlobalShortcuts 流程。
- 不实现 RemoteDesktop Portal / InputCapture / 模拟 Ctrl+C Portal 权限。
- 不实现截图翻译。
- 不实现 OCR。
- 不实现多模态图片翻译。
- 不强制实现 `TranslationPopup.qml`。
- 不重写整个设置页。
- 不让 QML 承担完整业务编排。
- 不恢复旧 `Controller` / 旧 `Hotkey` wrapper。

---

## 4. 推荐拆分

本阶段建议拆成四个小步骤实现。

```text
Phase 2A-1: 修复 SelectionTextProvider fallback
Phase 2A-2: 增加可靠触发入口
Phase 2A-3: 引入 TranslationCoordinator
Phase 2A-4: 改善快捷键失败提示
```

如果时间有限，至少完成 2A-1 和 2A-2，保证 Wayland 下有可测闭环。

---

## 5. Phase 2A-1：SelectionTextProvider fallback

### 5.1 当前问题

当前实现先写 clipboard marker，然后尝试模拟复制：

```text
requestSelectedText()
    ↓
写 marker 到 Clipboard
    ↓
simulateCopy()
    ↓
Wayland 下 simulateCopy() 返回 false
    ↓
失败
```

这会导致 Wayland 下完全不可用，并且会破坏用户原有 clipboard 内容，无法自然 fallback 到普通 Clipboard。

### 5.2 优化后策略

`SelectionTextProvider::requestSelectedText()` 必须按平台区分策略，避免把旧剪贴板内容误当成当前选区。

```text
Wayland:
1. 尝试 QClipboard::Selection / Primary Selection
2. 尝试 QClipboard::Clipboard / 普通剪贴板
   - 这里的 Clipboard 语义是“用户已经手动 Ctrl+C 后的 fallback”
3. 如果仍为空：给出“请先 Ctrl+C”提示，不模拟按键

Non-Wayland:
1. 优先走 ClipboardGuard + marker + simulateCopy，获取当前选区
2. 模拟复制失败或剪贴板未变化时，发出明确失败原因
3. 不应优先读取普通 Clipboard，避免翻译用户上一次复制的旧内容
```

如果后续需要“翻译剪贴板内容”，应作为单独动作（例如 `translateClipboardText()` 或 UI 文案 `Translate Clipboard`），不要混入 `translateSelectedText()` 的非 Wayland 默认路径。

### 5.3 推荐接口调整

`src/input/SelectionTextProvider.h`：

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

private:
    void requestSelectedTextBySimulatedCopy();
    bool simulateCopy() const;
};
```

### 5.4 推荐逻辑

```cpp
void SelectionTextProvider::requestSelectedText()
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        emit selectedTextFailed("Clipboard is unavailable");
        return;
    }

    const bool isWayland = QGuiApplication::platformName().contains("wayland", Qt::CaseInsensitive);

    if (isWayland) {
        if (clipboard->supportsSelection()) {
            const QString selected = clipboard->text(QClipboard::Selection).trimmed();
            if (!selected.isEmpty()) {
                emit selectedTextReady(selected);
                return;
            }
        }

        const QString copied = clipboard->text(QClipboard::Clipboard).trimmed();
        if (!copied.isEmpty()) {
            emit selectedTextReady(copied);
            return;
        }

        emit selectedTextFailed(
            "当前 Wayland 环境无法直接读取选中文本，请先 Ctrl+C 复制文本，再触发翻译。"
        );
        return;
    }

    requestSelectedTextBySimulatedCopy();
}
```

### 5.5 非 Wayland 模拟复制

旧的 marker + `ClipboardGuard` + `simulateCopy()` 逻辑只应放在：

```cpp
void SelectionTextProvider::requestSelectedTextBySimulatedCopy();
```

并且只在非 Wayland 环境调用。

要求：

- 不在 Wayland 下调用 XTest 或其他模拟输入。
- 非 Wayland 下应优先模拟复制当前选区，不应先读取普通 Clipboard，避免翻译旧剪贴板内容。
- marker 逻辑不应破坏用户原有剪贴板；成功或失败都应尽量恢复。
- 读取到空文本时不发起翻译。
- 失败时必须发出清晰 `selectedTextFailed(reason)`。

---

## 6. Phase 2A-2：可靠触发入口

Wayland 下全局快捷键可能注册失败，因此必须有 fallback 触发入口。

### 6.1 应用内按钮入口（必选）

必须在 `AppView.qml` 或主窗口中增加一个轻量按钮，例如：

```text
Translate Selected / Translate Clipboard
```

触发：

```qml
translationCoordinator.translateSelectedText()
```

原因：Wayland 下全局快捷键可能不可用，且不同桌面环境下托盘图标也可能不可见或不可用。应用内按钮是最可靠的手动 fallback。

该按钮用于验证 Wayland fallback：

```text
选中文本 → Ctrl+C → 应用内按钮 → 翻译
```

### 6.2 托盘菜单入口（推荐）

在 `qml/main.qml` 的 `SystemTrayIcon.menu` 中增加：

```qml
MenuItem {
    text: "Translate Selection"
    onTriggered: {
        translationCoordinator.translateSelectedText()
    }
}
```

如果本阶段尚未引入 `TranslationCoordinator`，可以临时调用：

```qml
selectionTextProvider.requestSelectedText()
```

但最终应改为 coordinator。托盘入口只能作为补充入口，不能替代应用内按钮。

---

## 7. Phase 2A-3：新增 TranslationCoordinator

### 7.1 新增文件

```text
src/app/TranslationCoordinator.h
src/app/TranslationCoordinator.cpp
```

### 7.2 职责

`TranslationCoordinator` 负责应用级流程编排：

- 响应 `translateSelection` action。
- 调用 `SelectionTextProvider` 获取文本。
- 获取文本成功后通知 UI：显示主窗口、填充输入框、开始翻译。
- 获取文本失败后通知 UI 展示错误或提示。

它不负责：

- 快捷键注册。
- clipboard / primary selection 具体读取细节。
- 网络请求实现。
- Provider 配置保存。
- QML 组件绘制。

### 7.3 最小推荐接口

```cpp
class SelectionTextProvider;

class TranslationCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit TranslationCoordinator(QObject* parent = nullptr);

    Q_INVOKABLE void setSelectionTextProvider(QObject* provider);
    Q_INVOKABLE void translateSelectedText();
    Q_INVOKABLE void showNotification(QString message);

signals:
    void requestShowMainWindow();
    void requestSetInputText(QString text);
    void requestStartTranslation(QString source);
    void requestShowNotification(QString message);
    void requestShowError(QString message);

private:
    QPointer<SelectionTextProvider> m_selectionTextProvider;
};
```

说明：

- 使用 `QObject*` 作为 QML 注入参数更方便，内部 `qobject_cast<SelectionTextProvider*>`。
- 如果项目选择 C++ 创建全局对象，也可以改为构造函数注入。
- 本阶段无需让 coordinator 直接持有 `TranslationService`；可继续复用 `AppView.startTrans()`，降低改动风险。

### 7.4 行为要求

`translateSelectedText()`：

```text
1. 检查 SelectionTextProvider 是否存在
2. 调用 requestSelectedText()
3. selectedTextReady(text):
   - trim 后为空则 requestShowNotification，不翻译
   - requestSetInputText(text)
   - requestShowMainWindow()
   - requestStartTranslation("selectedText")
4. selectedTextFailed(reason):
   - requestShowNotification(reason) 或 requestShowError(reason)
```

### 7.5 CMake

新增文件后更新：

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

### 7.6 main.cpp

注册 QML 类型：

```cpp
qmlRegisterType<TranslationCoordinator>("TransAI", 1, 0, "TranslationCoordinator");
```

---

## 8. Phase 2A-4：快捷键失败提示

当前快捷键失败不能只 `console.warn`。至少应把状态展示给用户。

### 8.1 QML 行为

在 `qml/main.qml` 中：

```qml
GlobalShortcutManager {
    id: shortcutManager
    onShortcutActivated: function(actionId) {
        if (actionId === "translateSelection") {
            translationCoordinator.translateSelectedText()
        }
    }
    onShortcutRegisterFailed: function(actionId, reason) {
        console.warn("Shortcut registration failed", actionId, reason)
        translationCoordinator.showNotification(
            "当前环境无法注册全局快捷键，请使用应用内按钮/托盘菜单，或先 Ctrl+C 后再触发翻译。"
        )
    }
}
```

实际 QML 中不能直接调用 signal 时，可改为调用 coordinator 的 invokable helper，或直接设置 `appView.statusText`。

### 8.2 用户提示建议

Wayland 下推荐提示：

```text
当前 Wayland 环境暂不支持应用直接注册全局快捷键。
请使用托盘菜单，或先 Ctrl+C 复制文本后触发翻译。
```

非 Wayland 下推荐提示：

```text
快捷键注册失败，可能与其他应用冲突，请更换快捷键。
```

---

## 9. QML 集成推荐结构

`qml/main.qml` 最终推荐结构：

注意：QML 不能直接调用 C++ signal；需要调用 `showNotification(...)` 这类 `Q_INVOKABLE` helper，或直接更新 `appView.statusText`。

```qml
SelectionTextProvider {
    id: selectionTextProvider
}

TranslationCoordinator {
    id: translationCoordinator

    Component.onCompleted: {
        setSelectionTextProvider(selectionTextProvider)
    }

    onRequestSetInputText: function(text) {
        appView.inputText = text
    }

    onRequestStartTranslation: function(source) {
        appView.startTrans(source)
    }

    onRequestShowMainWindow: {
        mainWindow.show()
        mainWindow.raise()
        mainWindow.requestActivate()
        ensureWindowVisible()
    }

    onRequestShowNotification: function(message) {
        console.warn(message)
        appView.statusText = message
    }

    onRequestShowError: function(message) {
        console.warn(message)
        appView.statusText = message
    }
}

GlobalShortcutManager {
    id: shortcutManager
    onShortcutActivated: function(actionId) {
        if (actionId === "translateSelection") {
            translationCoordinator.translateSelectedText()
        }
    }
}
```

QML 只负责展示和转发，不负责 clipboard fallback、快捷键 backend、翻译服务内部逻辑。

---

## 10. TranslationRequest 构造

建议把 `AppView.startTrans()` 改成支持来源参数，避免后续 OCR / 图片翻译再次修改调用链：

```qml
function startTrans(source) {
    var requestSource = source || "manual"
    if (inputArea.text.length === 0 || !transBtn.visible) {
        return
    }
    translationService.translateFromMap({
        "text": inputArea.text,
        "targetLanguage": langSelector.currentText,
        "mode": getMode(),
        "source": requestSource,
        "provider": setting.provider,
        "apiServer": setting.apiServer,
        "apiKey": setting.apiKey,
        "model": setting.model
    })
}
```

手动按钮继续调用：

```qml
appView.startTrans("manual")
```

划词翻译由 coordinator 触发：

```qml
onRequestStartTranslation: function(source) {
    appView.startTrans(source) // source = "selectedText"
}
```

这样 Phase 2B 可以复用同一路径触发 `source = "screenshotOcr"`，无需让 `TranslationCoordinator` 直接持有 `TranslationService`。

---

## 11. Wayland 全局快捷键后置说明

本阶段不实现完整 `PortalShortcutBackend`。原因：

- `xdg-desktop-portal GlobalShortcuts` 是异步 D-Bus 流程。
- 用户可能拒绝授权。
- 不同桌面环境和 portal backend 支持度不同。
- Portal 的 `preferred_trigger` 只是建议，不是应用强制绑定。

Wayland 下真正的全局快捷键应在 Phase 2D 中实现：

```text
PortalShortcutBackend
    ↓
CreateSession
    ↓
BindShortcuts(preferred_trigger)
    ↓
用户通过系统 UI 确认或修改快捷键
    ↓
Activated(session_handle, shortcut_id, timestamp, options)
    ↓
GlobalShortcutManager.shortcutActivated("translateSelection")
```

Phase 2A 必须保留 fallback，不得依赖 Portal 成功。

---

## 12. 设置兼容

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

本阶段不强制新增复杂配置结构。若新增 `shortcuts.translateSelection`，必须：

```text
1. 旧 shortCut 仍可读取
2. shortcuts.translateSelection 不存在时使用 shortCut
3. 保存时可同时写新旧字段
4. 不改变配置文件路径
```

---

## 13. Review 重点

改动后检查：

```bash
rg -n "src/controller|src/hotkey|class Controller|class Hotkey|APIController|Hotkey \{" src qml CMakeLists.txt
```

预期：无命中。第三方 QHotkey 库和 `QHotkeyShortcutBackend` 命中是允许的。

检查架构边界：

```bash
rg -n "TranslationService|translateFromMap|clipboard|QClipboard|simulateCopy|XTest" src/input src/app qml
```

重点确认：

- `GlobalShortcutManager` 不读取剪贴板。
- `QHotkeyShortcutBackend` 不读取剪贴板、不调用翻译。
- `SelectionTextProvider` 不调用 `TranslationService`。
- `TranslationCoordinator` 不做底层 clipboard 操作。
- QML 不实现复杂 clipboard fallback。

构建：

```bash
cmake --build build -j$(nproc)
```

---

## 14. 验收清单

### 14.1 功能验收

- [ ] 手动输入翻译不退化。
- [ ] `translateSelection` action 仍存在。
- [ ] 应用内按钮触发方式可用，且不依赖全局快捷键或托盘。
- [ ] 快捷键或托盘菜单作为补充触发方式可用时不退化。
- [ ] Wayland 下快捷键注册失败时有提示。
- [ ] 应用内按钮和托盘菜单可触发划词翻译流程。
- [ ] 触发后进入 `TranslationCoordinator.translateSelectedText()`。
- [ ] 能尝试读取 Primary Selection。
- [ ] Wayland 下 Primary Selection 为空时 fallback 到 Clipboard。
- [ ] Wayland 下 Clipboard 有文本时可翻译。
- [ ] Wayland 下 Clipboard 为空时给出手动 Ctrl+C 提示。
- [ ] 非 Wayland 下优先尝试模拟 Ctrl/Cmd+C 获取当前选区。
- [ ] 非 Wayland 下不优先翻译旧 Clipboard 内容。
- [ ] 获取文本为空时不发起翻译。
- [ ] 获取文本成功后能通过 `TranslationService` 翻译。
- [ ] 结果能在主窗口展示。
- [ ] Wayland 下能力不可用时不崩溃、不静默失败。

### 14.2 架构验收

- [ ] `GlobalShortcutManager` 不读取剪贴板。
- [ ] `QHotkeyShortcutBackend` 不读取剪贴板。
- [ ] `SelectionTextProvider` 不调用翻译服务。
- [ ] `TranslationService` 不知道快捷键来源。
- [ ] `TranslationCoordinator` 负责划词翻译流程编排。
- [ ] QML 只负责展示、转发和调用公开接口。
- [ ] 未实现截图 / OCR / 多模态。
- [ ] 未恢复旧 `Controller` / 旧 `Hotkey` wrapper。

### 14.3 构建验收

至少执行：

```bash
cmake --build build -j$(nproc)
```

如果没有现成 build 目录，可按项目 `AGENTS.md` 中的方式配置构建。

---

## 15. 推荐提交拆分

```text
feat(input): add selection text fallback strategies
feat(qml): add translate selection fallback triggers
feat(app): add translation coordinator for selected text flow
feat(qml): show shortcut registration fallback guidance
```

如果把 AppView 的 selectedText source 区分也纳入本阶段：

```text
feat(qml): mark selected text translation source
```

---

## 16. 注意事项

1. 不要把 Portal Shortcut 放进 Phase 2A 的必要路径。
2. 不要把 RemoteDesktop Portal 放进 Phase 2A。
3. 不要在 Wayland 下默认模拟 Ctrl+C。
4. 不要为了划词翻译重写整个 `AppView.qml`。
5. Wayland 下失败时要给出清晰提示，而不是静默失败。
6. 第一版以“可用闭环”为目标，Popup、高级 Portal 和多模态后续再做。

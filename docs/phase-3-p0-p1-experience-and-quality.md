# Phase 3 设计方案：P0/P1 体验与质量改进

> 目标读者：负责实现的 coding agent
> 适用项目：TransAI / Qt6 / QML / C++17
> 前置条件：已完成 Phase 1、Phase 2A、截图 vision 翻译（见 AGENTS.md）

---

## 0. 范围与来源

本方案覆盖此前代码评审确定的全部 P0 / P1 项：

| 编号 | 优先级 | 事项 | 本文章节 |
|---|---|---|---|
| P0-1 | P0 | 结果面板复制按钮 | §3 |
| P0-2 | P0 | 目标语言持久化 | §4 |
| P0-3 | P0 | 系统代理支持 | §5 |
| P1-1 | P1 | C++ 错误消息 i18n | §6 |
| P1-2 | P1 | CI 流水线 | §7 |
| P1-3 | P1 | ExplainWord / CorrectGrammar UI 入口 | §8 |

非目标（本阶段不做，仅记录）：翻译历史、密钥安全存储（keychain/DPAPI）、
OCR 截图翻译闭环、StreamingChatClient 基类抽取、设置页拆分、
`useSystemProxy` 用户开关。

实施顺序建议：P0-3 → P0-2 → P0-1 → P1-3 → P1-1 → P1-2。前三项互不依赖，
P1-3 复用 P0-2 引入的持久化模式，P1-1 纯增量。

---

## 1. 执行前必读

- `AGENTS.md`
- `src/settings/Setting.h/cpp`（Q_PROPERTY_AUTO 与 config 读写模式）
- `qml/AppView.qml`、`qml/I18n.qml`、`qml/ThemedComboBox.qml`
- `src/translation/TranslationRequest.cpp`（`translationModeFromInt` 的 0/1/2 映射）
- `src/main.cpp`
- `tests/CMakeLists.txt`

## 2. 共同约定

- **configVersion 不升级**：本阶段所有新增配置 key（`targetLanguage`、
  `translationMode`）均为可选 key。新二进制读旧配置得到空/默认值；旧二进制
  读新配置忽略未知 key。双向兼容，无需迁移逻辑。
- **i18n 机制不变**：继续使用 `I18n.t()` 的"英文原文作 key"模式，新增词条
  只追加到 `_zhTable`，不引入错误码枚举。
- **Setting 新属性**：用 `Q_PROPERTY_AUTO`，setter 不自动写盘；由 QML 侧
  显式触发 `setting.updateConfig()`（离散低频操作可不经防抖，与
  SettingView 的 `saveConfigSoon()` 模式区分）。
- 每个章节末尾列出验收标准；§9 汇总测试策略。

---

## 3. P0-1：结果面板复制按钮

### 现状

`AppView.qml` 的结果面板（`GTextEdit result`，readOnly）只能手动全选复制；
QML 标准库没有"直接写剪贴板"的 API（`TextEdit.copy()` 只复制选区）。

### 设计

**C++ 侧**：新增 `src/app/ClipboardHelper.h/cpp`：

```cpp
class ClipboardHelper : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    // Writes text to the system clipboard. Named setText (not copy) to mirror
    // QClipboard; exposed as the "clipboard" context property.
    Q_INVOKABLE void setText(const QString& text);
};
```

实现为 `QGuiApplication::clipboard()->setText(text)`（`#include <QClipboard>`）。
不触碰 `ClipboardGuard`（那是划词翻译的 RAII 守卫，用途不同）。

`main.cpp`：`new ClipboardHelper(&app)`，注册 context property `clipboard`。
不注册 QML type——单实例全局对象，context property 最贴合现有模式
（`setting`、`windowDismiss` 同款）。

**QML 侧**（`AppView.qml`）：在 "Translated" 指示器（`indictor`）所在行右侧、
`pinButton` 左侧放一个小型文本按钮，视觉风格与 ✕ 关闭按钮一致
（`mutedForeground` → hover `accentHover`）：

- 文案：`I18n.t("Copy")`，点击后 1.5s 内显示 `I18n.t("Copied ✓")`
  （`Timer` 恢复，仿 SettingView 的 "Saved ✓" 反馈）。
- 点击：`clipboard.setText(result.text)`。
- 可见性/可用性：`result.text.length > 0` 时可用；流式中途允许复制
  （复制当前部分内容是有意行为）。截图会话同样受益（共用 result 面板）。

**i18n 新增**：`"Copy" → "复制"`、`"Copied ✓" → "已复制 ✓"`。

### 验收

- 有结果时点击按钮，粘贴到外部编辑器内容一致；1.5s 后按钮文案恢复。
- 结果为空时按钮禁用/隐藏，不产生空剪贴板写入。

---

## 4. P0-2：目标语言持久化

### 现状

`AppView.qml` 中 `langSelector.currentIndex: 0` 写死，每次启动重置为
"简体中文"。

### 设计

**Setting**：

- `Setting.h` 加 `Q_PROPERTY_AUTO(QString, targetLanguage);`（成员
  `_targetLanguage`，默认空串 = 未设置）。
- `Setting.cpp`：
  - 常量 `kTargetLanguageKey[] = "targetLanguage";`
  - `loadConfig()`：`_targetLanguage = obj.value(kTargetLanguageKey).toString();`
  - `updateConfig()`：写入该 key。

**AppView.qml**：

```qml
ThemedComboBox {
    id: langSelector
    model: ["简体中文", "繁体中文", "English", "Japanese", "German", "Korean", "Español", "français"]
    Component.onCompleted: {
        const idx = find(setting.targetLanguage)
        if (idx >= 0) currentIndex = idx
        // 找不到（含空串）保持默认 0
    }
    onActivated: {
        setting.targetLanguage = currentText
        setting.updateConfig()   // 低频离散操作，直接写盘
    }
}
```

注意用 `onActivated`（用户主动选择）而非 `onCurrentIndexChanged`，避免
`Component.onCompleted` 的恢复赋值触发二次写盘。

### 验收

- 切换到 "English" → 重启应用 → 语言选择仍为 "English"。
- 手工把 config 中 `targetLanguage` 改成非法值 → 启动回落到 index 0 且不崩溃。

---

## 5. P0-3：系统代理支持

### 现状

全代码库未调用 `QNetworkProxyFactory::setUseSystemConfiguration()`，Qt 默认
不使用系统代理；企业/校园网环境下云端 API 完全不可达。

### 设计

`src/main.cpp`，在 `QGuiApplication app(argc, argv);` 之后立即：

```cpp
// Honour the OS proxy configuration ( corporate / campus networks ).
// localhost queries resolve to Direct on all supported platforms, so local
// Ollama is unaffected.
QNetworkProxyFactory::setUseSystemConfiguration(true);
```

（`#include <QNetworkProxyFactory>`）

**影响评估**：

- Windows / macOS / GNOME 的系统代理设置均默认绕过 localhost，Ollama
  本地调用不受影响。
- 无系统代理的环境返回 Direct，行为与现状完全一致。
- 用户级开关（`useSystemProxy` config key + 设置页 checkbox）列为可选后续，
  本阶段不做。

### 验收

- 设置系统代理（如本机 `127.0.0.1:8888` 抓包工具）后，OpenAI 请求经过代理。
- Ollama localhost 请求直连不经过代理。
- 无代理环境下全部行为与改动前一致。

---

## 6. P1-1：C++ 错误消息 i18n

### 现状

C++ 服务把英文错误串写入 `responseError` / `captureFailed` / `selectedTextFailed`，
QML 原样显示（`result.text = responseError`），中文界面下仍是英文。

### 设计

利用 I18n 既有"英文原文作 key、未命中回退原文"的机制，**不改任何 C++ 接口**：

**第一步：补全 `_zhTable`**（以下清单已逐条核对源码，实施时以 grep 复核为准）：

| 来源 | 英文原文（key） |
|---|---|
| TranslationService | `Please enter text to translate` |
| TranslationService | `Please select a model` |
| ProviderDefaults | `Please provide the correct apikey` |
| TranslationService | `Translation timed out: no data from the server` |
| TranslationService | `Network error: could not reach the server` |
| TranslationService | `Proxy error: could not reach the server through the proxy` |
| TranslationService | `Request rejected: check the API key, model name and server URL` |
| TranslationService | `Protocol error: unexpected response from the server` |
| TranslationService | `Server error: the API server failed to handle the request` |
| TranslationService | `Network error`（兜底） |
| VisionTranslationService | `No screenshot image was provided` |
| ImagePayloadBuilder | `The captured image is empty` |
| ImagePayloadBuilder | `The captured image is too small` |
| ImagePayloadBuilder | `Failed to encode the image as JPEG` |
| SelectionTextProvider | `Clipboard is unavailable` |
| SelectionTextProvider | `Failed to simulate copy` |
| SelectionTextProvider | `Clipboard did not change after copy` |
| SelectionTextProvider | `No selected text found` |
| ScreenshotCaptureService | `Screen capture is not available on this platform`（已有词条，复用） |
| ScreenshotCaptureService | `The selected region is outside the captured screen` |

另需核对 `VisionTranslationService` 的 `kVisionUnsupportedGuidance` 全文
（多行指引串），作为整体加入词条。

**第二步：QML 显示点包 `I18n.t()`**（共 4 处，均在 `AppView.qml`）：

- `translationService.onResponseErrorChanged` / `visionService.onResponseErrorChanged`：
  `result.text = I18n.t(responseError)`
- `showScreenshotError(message)` / `showSelectionError(message)`：
  `result.text = I18n.t(message)`

服务端动态返回的错误（`ApiError` 提取的 payload message，如限流详情）不在
表中，`t()` 原样回退——这是有意行为， provider 原文对排查更有用。

### 已知限制（写入 docs，不改代码）

错误显示后再切换界面语言不会重新翻译（赋值而非绑定）。错误提示是瞬态的，
重开窗口即消失，可接受。

### 验收

- 中文界面下：清空输入直接翻译、断网翻译、选错模型截图翻译，均显示中文错误。
- 英文界面下全部显示原英文。
- 服务端返回的英文动态错误（如 401 body message）两种语言下都原样显示。

---

## 7. P1-2：CI 流水线

### 设计

新增 `.github/workflows/ci.yml`（Linux 先行，Windows/macOS 构建列为后续扩展）：

```yaml
name: CI
on:
  push:
    branches: [main]
  pull_request:

jobs:
  linux:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Install system dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libx11-dev libxtst-dev libgl1-mesa-dev \
            libxkbcommon-dev libdbus-1-dev
      - name: Install Qt
        uses: jurplel/install-qt-action@v4
        with:
          version: '6.9.*'
          cache: true
      - name: Configure & build
        run: |
          cmake -B build -D CMAKE_BUILD_TYPE=Release
          cmake --build build -j$(nproc)
      - name: Unit tests
        run: ctest --test-dir build --output-on-failure
      - name: Headless smoke test
        run: |
          QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/TransAI --background &
          sleep 3
          QT_QPA_PLATFORM=offscreen ./build/TransAI --translate-selection
          kill %1
```

要点：

- Qt 6.9.x（满足 `QTP0001` 的 6.5+ 分支与 AGENTS.md 的 6.9+ 约定）；
  `install-qt-action` 自带缓存。
- smoke test 复用 AGENTS.md 记录的 offscreen 路径：第一实例驻留，
  第二实例转发命令后退出；验证单实例 + QML 加载不崩溃。
  （offscreen 下 GlobalShortcutManager 按设计拒绝注册，属预期日志。）
- 失败排查：smoke 步骤加 `QT_FORCE_STDERR_LOGGING=1`，避免 qDebug 进
  journal 看不到。

### 验收

- PR 触发流水线，构建 + 6 个单测 + smoke 全绿。
- 故意引入编译错误的分支能正确变红。

---

## 8. P1-3：ExplainWord / CorrectGrammar UI 入口

### 现状

`PromptBuilder` 已实现三个模式的完整 prompt；`translationModeFromInt` 已支持
0/1/2；但 `AppView.getMode()` 写死 `return 0`（两处 `TODO(phase-2c)`）。

### 设计

**Setting**（与 P0-2 同模式）：

- `Q_PROPERTY_AUTO(int, translationMode);` 成员 `_translationMode = 0`。
- config key `translationMode`（int）；`loadConfig()` 读取并钳制到 0–2，
  越界归 0。

**AppView.qml**：输入区上方新增分段控件 `modeBar`（三个 pill 段：
Translate / Explain Word / Correct Grammar）。

> 实现说明（与原设计的偏差）：原设计在底部栏 `langSelector` 右侧放
> ThemedComboBox。实现时测算发现底部栏在默认宽度 460px（最小 400px）下
> 放不下"语言框 + 模式框 + 翻译按钮"三件套（英文最长文案约溢出 20–50px），
> 且 ComboBox 在语言切换导致 model 重建时需额外恢复 currentIndex。
> 分段控件选中态直接绑定 `setting.translationMode`，配置恢复与语言切换
> 零命令式代码，且布局无压力。

```qml
// 选中态即绑定，点击写回 setting 并持久化
onClicked: {
    setting.translationMode = index
    setting.updateConfig()
}
```

- `getMode()` 改为 `return setting.translationMode`，删除两处
  `TODO(phase-2c)` 注释。
- `modeBar.modes` 是绑定，界面语言切换时词条实时更新；选中态由
  `setting.translationMode` 绑定驱动，不受 Repeater 重建影响。

**联动文案**（低成本、收益明确的 polish）：

- 翻译按钮：按模式显示 `Translate` / `Explain` / `Correct`（保留快捷键后缀），
  用绑定表达式而非命令式赋值。
- "Translated" 指示器：按模式显示 `Translated` / `Explanation` / `Corrected`。

**i18n 新增**：`"Translate" → "翻译"`、`"Explain Word" → "单词讲解"`、
`"Correct Grammar" → "语法纠错"`、`"Explain (Ctrl+Enter)" → "讲解 (Ctrl+Enter)"`、
`"Correct (Ctrl+Enter)" → "纠错 (Ctrl+Enter)"`（macOS 变体同理）、
`"Explanation" → "讲解结果"`、`"Corrected" → "纠错结果"`。

**边界说明**（写入代码注释）：截图 vision 路径不使用 `PromptBuilder`，
`modeSelector` 只影响文本翻译；`TranslationRequest` 不含 mode 的截图请求
行为不变。

### 验收

- 三种模式下 prompt 分别命中 `PromptBuilder` 三个分支（可用本地 mock
  endpoint 或日志确认请求体）。
- 模式选择重启后保持。
- 切换界面语言后模式下拉文案跟随、选中项不变。

---

## 9. 测试策略

### 新增单测：`tests/tst_setting.cpp`

P0-2 与 P1-3 都改 Setting 持久化，补一个往返测试（此前 Setting 无覆盖）：

- `QStandardPaths::setTestModeEnabled(true)` 隔离配置目录；
- 构造 Setting → 改 `targetLanguage` / `translationMode` → `updateConfig()`
  → 析构 → 重新构造 → 断言读回；
- 非法值用例：手写 config 注入越界 `translationMode`（如 7）与未知
  `targetLanguage`，断言回落默认。

`tests/CMakeLists.txt` 按现有模式追加（需链接 `Qt6::Test` + Setting.cpp +
ProviderEndpointBuilder 等传递依赖；若链接链过长，允许把 Setting 依赖的
providers 源文件一并列入，参照既有 target 写法）。

### 手动验收清单

- §3–§8 各章节"验收"小节逐条过；
- 冒烟：offscreen 双进程命令转发不崩溃；
- 回归：流式翻译 / 停止 / 截图翻译 / 划词翻译各跑一遍。

## 10. 改动文件清单

| 文件 | 改动 |
|---|---|
| `src/app/ClipboardHelper.h/cpp` | 新增（P0-1） |
| `src/main.cpp` | 注册 `clipboard` context property；系统代理一行（P0-1/P0-3） |
| `src/settings/Setting.h/cpp` | `targetLanguage`、`translationMode` 两个 key（P0-2/P1-3） |
| `qml/AppView.qml` | 复制按钮、语言/模式恢复与保存、modeSelector、联动文案、错误显示包 i18n（P0-1/P0-2/P1-1/P1-3） |
| `qml/I18n.qml` | `_zhTable` 追加约 30 条（全部章节） |
| `CMakeLists.txt` | SOURCES/HEADERS 加 ClipboardHelper（P0-1） |
| `tests/tst_setting.cpp`、`tests/CMakeLists.txt` | 新增 Setting 往返测试（§9） |
| `.github/workflows/ci.yml` | 新增（P1-2） |
| `AGENTS.md` | Settings Storage 段落补两个新 key；P1-1 的"已知限制"记录 |

预计总工作量：1–1.5 天（含 CI 调试）。

# Phase 2E 实施任务书：多模态截图翻译（Vision-First）

> 目标读者：负责实现截图翻译的 coding agent  
> 适用项目：TransAI / Qt6 / QML / C++17  
> 前置条件：Phase 1 已完成（当前代码基线即满足）  
> 与既有文档的关系：本文取代 `phase-2b` 中"必须先走 tesseract OCR"的默认假设，
> 将**多模态直连**作为默认推荐路径；`phase-2b` 的 OCR 路径降级为可选的离线后备方案，
> `phase-2d` 中的 Vision 部分由本文细化为可执行计划。

---

## 1. 设计出发点：让用户"零配置可用"

两条技术路线对用户的配置成本完全不同：

| 路线 | 用户需要的额外配置 |
|---|---|
| OCR（tesseract 外部进程） | 安装 tesseract + 语言包、配置命令路径、配置语言组合 |
| 多模态直连（vision chat） | **几乎没有**：复用已有 provider / apiServer / apiKey，只需一个"看得懂图"的模型 |

因此本阶段的核心原则是：

```text
默认路径 = 截图 → 多模态大模型 → 流式翻译结果
配置目标 = 大多数用户什么都不填就能用；填也只填一个模型名
```

实现"零配置"的三个手段：

1. **凭证复用**：vision 请求与文本翻译共用 provider / apiServer / apiKey，
   不引入第二套服务器配置。
2. **模型跟随**：`screenshotVisionModel` 默认为空 = 跟随当前翻译模型；
   只有当前模型不具备视觉能力时，才需要用户单独指定。
3. **能力探测 + 主动引导**：用启发式规则判断模型是否支持视觉，
   设置页实时提示；用户选错模型时在翻译时报清晰错误并引导回设置页，
   而不是静默失败。

---

## 2. 本阶段目标 / 非目标

### 2.1 目标

```text
触发（快捷键 / 托盘菜单 / 应用内按钮 / CLI --translate-screenshot）
    ↓
ScreenshotCaptureService（Wayland: Portal；X11/Win/mac: QScreen 兜底）
    ↓
QImage
    ↓
VisionTranslationService（ImagePayloadBuilder 压缩编码 → OpenAI-compatible vision payload）
    ↓
复用 StreamParser / ProviderEndpointBuilder / ApiError / ThinkingPolicy
    ↓
主窗口 AppView 流式展示结果
```

### 2.2 非目标（本阶段不做）

- 不实现 OCR / tesseract 路径（保留配置位，后续按 phase-2b 补）。
- 不实现独立的悬浮结果窗；结果复用主窗口。
- 不把图片 payload 混进 `PromptBuilder` / `TranslationService`。
- 不支持 Ollama 非兼容 vision 字段（Ollama 走 OpenAI 兼容端点即可）。

---

## 3. 配置设计（用户视角先行）

### 3.1 配置项

沿用 `Setting` 现有扁平键风格（per-provider 键 + 默认值兜底），新增：

```json
"screenshotShortCut": "",            // 截图翻译快捷键，空 = 未设置（不抢占）
"screenshotTranslateMode": "auto",   // auto | vision | ocr；ocr 本阶段暂不可用
"screenshotVisionModel": "",         // 空 = 跟随当前翻译模型
"screenshotMaxImageWidth": 1600,     // 高级项
"screenshotJpegQuality": 85          // 高级项
```

约束：

- `screenshotTranslateMode` 默认 `auto`：视觉能力可用走 vision，
  否则报清晰错误并引导设置（OCR 落地后再加 fallback）。
- `screenshotVisionModel` 为空时绝不误走 vision 以外的路径。
- 旧配置键（`shortCut`、`openai*`、`ollama*` 等）加载行为完全不变；
  旧 config.json 无新键时按上述默认值工作，即升级后行为 = 未启用截图翻译。

### 3.2 设置页 UX（SettingView 新增"截图翻译"卡片）

复用现有 `SettingsCard` / `ThemedComboBox` / `ThemeColors` 组件：

```text
┌ 截图翻译 ──────────────────────────────┐
│ 模式        [自动(推荐) ▾]              │
│ 视觉模型    [跟随当前模型 ▾]  ← 模型列表 │
│             复用 ModelDiscoveryService  │
│ ⚠ 当前模型 xxx 可能不支持图片输入，     │
│   建议单独选择一个视觉模型               │  ← 仅探测失败时显示
│ 截图快捷键  [ Click to record ]         │
│ ▸ 高级（图片尺寸上限 / JPEG 质量）       │  ← 默认折叠
└────────────────────────────────────────┘
```

要点：

- 模型下拉框复用现有"检测模型"流程，不做第二套网络逻辑。
- 视觉能力提示是**非阻断性**警告：启发式可能误判，用户仍可保存。
- 所有字符串走 `I18n.t(...)` 并补 `_zhTable`。

### 3.3 视觉能力探测

新增 `src/vision/VisionModelCapability.h`（纯函数，可单测）：

```cpp
namespace VisionModelCapability {
    // 启发式：模型 id 含已知视觉标记即视为支持。只用于 UI 提示与 auto 模式
    // 路由，不作为硬门禁——服务端 400 时按"模型不支持图片"报清晰错误。
    bool isLikelyVisionModel(const QString& modelId);
}
```

已知视觉标记（初版，可扩充）：
`gpt-4o`、`gpt-4.1`、`gpt-5`、`o4`、`vision`、`-vl`（qwen-vl / glm-4v）、
`gemini`、`claude`、`llava`、`minicpm-v`、`pixtral`、`qwen2.5-vl`、`doubao-vision`。

`tests/` 新增 `tst_visionmodelcapability.cpp` 覆盖常见模型 id。

---

## 4. 模块划分

```text
src/capture/
    ScreenshotCaptureService.h/.cpp   # 统一入口：captureInteractive()
    PortalScreenshotBackend.h/.cpp    # Wayland: org.freedesktop.portal.Screenshot (Qt DBus)
src/vision/
    ImagePayloadBuilder.h/.cpp        # QImage → 缩放/JPEG/base64 data URL → payload JSON
    VisionTranslationService.h/.cpp   # 流式 vision chat 请求（信号与 TranslationService 对齐）
    VisionModelCapability.h           # 纯函数启发式探测
```

职责边界（验收红线）：

- `ScreenshotCaptureService` 只产出 `QImage`，不认识 OCR 和翻译。
- `ImagePayloadBuilder` 只做图片处理与 payload 拼装，不触网。
- `VisionTranslationService` 独立于 `TranslationService`；两者**不互相持有**。
- `PromptBuilder` / `TranslationService` / `StreamParser` 零改动或最小改动
  （StreamParser 解析 SSE 的逻辑直接复用）。

### 4.1 ScreenshotCaptureService

```cpp
class ScreenshotCaptureService : public QObject
{
    Q_OBJECT
public:
    explicit ScreenshotCaptureService(QObject* parent = nullptr);
    Q_INVOKABLE void captureInteractive();
signals:
    void imageCaptured(QImage image);
    void captureCanceled();            // 用户取消 ≠ 错误
    void captureFailed(QString reason);
};
```

平台策略：

| 平台 | 后端 |
|---|---|
| Wayland | Portal Screenshot（系统 UI 交互选区/全屏），需 `Qt6::DBus` |
| X11 / Windows / macOS | 全屏抓取每块屏幕 → 应用内全屏选区覆盖层（`RegionSelectorWindow`，拖拽框选 / Esc·右键取消）→ 裁剪原图 |

- Portal 不可用 / 用户拒绝 → `captureFailed`（清晰原因）或 `captureCanceled`。
- 截图失败一律不崩溃。

### 4.2 ImagePayloadBuilder

```cpp
struct ImagePayloadResult {
    bool ok = false;
    QString error;                // ok=false 时给可读原因
    QByteArray base64DataUrl;     // "data:image/jpeg;base64,..."
    QSize finalSize;
};

class ImagePayloadBuilder
{
public:
    static ImagePayloadResult buildDataUrl(const QImage& image,
                                           int maxWidth, int jpegQuality);
};
```

- 宽度超过 `maxWidth` 按比例缩放（`Qt::SmoothTransformation`）。
- 一律编码为 JPEG（`quality` 可配），含透明通道时先铺白底。
- 空图 / 编码失败返回 `ok=false` + 原因。

### 4.3 VisionTranslationService

信号形状与 `TranslationService` 对齐，QML 绑定代码可镜像复用：

```cpp
class VisionTranslationService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString responseData READ responseData NOTIFY responseDataChanged)
    Q_PROPERTY(QString responseError READ responseError NOTIFY responseErrorChanged)
    Q_PROPERTY(bool isRequesting READ isRequesting NOTIFY isRequestingChanged)
public:
    explicit VisionTranslationService(QObject* parent = nullptr);
    Q_INVOKABLE void translateImage(const QImage& image, const QVariantMap& options);
    Q_INVOKABLE void abort();
signals:
    void translationStarted();
    void translationFinished(QString result);
    void translationFailed(QString error);
    void translationStopped();
    ...
};
```

请求构造（复用现有设施）：

- URL：`ProviderEndpointBuilder::buildChatCompletionUrl(apiServer, provider)`。
- Header / key 检查：与 `TranslationService` 相同（`ProviderDefaults`）。
- Body：OpenAI 兼容格式 —— `messages[0].content` 为数组：
  `{"type":"text","text": <翻译指令>}` + `{"type":"image_url","image_url":{"url": <data URL>}}`。
  翻译指令在 vision 模块内自建（简短 system+user：提取图中文字并翻译为目标语言，
  只输出译文），**不调用** `PromptBuilder`。
- 流式解析：复用 `StreamParser`；超时 / 中断 / 终止信号语义与文本翻译一致
  （一次请求只发一个终止信号）。
- 思考抑制：`ThinkingPolicyBuilder` 仍按模型 id 注入参数，复用现有的
  "被拒后自动重试"机制；`<think>` 残留由 `StreamParser` 既有逻辑剥离。
- 服务端返回"不支持图片 / invalid image"类 400 时，错误文案改写为
  "当前模型可能不支持图片输入，请到设置页选择视觉模型"并附原始信息。

### 4.4 视觉模型解析优先级

```text
screenshotVisionModel 非空 → 用它
否则 → 当前 provider 的模型（openaiModel / ollamaModel，含默认值兜底）
```

---

## 5. 触发链路

### 5.1 CLI / 单实例

`main.cpp` 新增：

```text
--translate-screenshot   → 转发命令 "translate-screenshot"（与 translate-selection 同一机制）
```

`qml/main.qml::handleApplicationCommand()` 增加分支：截图进行中先藏主窗口
（避免把自己截进去）→ `captureInteractive()` → 完成后
`requestShowMainWindow` + 结果流入 AppView。

### 5.2 触发入口（全部指向同一链路）

1. 托盘菜单："截图翻译"（i18n）。
2. AppView 输入区图标按钮（截图 / 图片占位，v1 至少截图）。
3. 全局快捷键 `screenshotShortCut`（X11 经 QHotkey；Wayland 提示用户
   自行把桌面快捷键绑到 `TransAI --translate-screenshot`，与划词一致）。
4. CLI `--translate-screenshot`（Wayland 用户的主要入口）。

### 5.3 AppView 展示

- `visionService` 与 `translationService` 在 QML 中并存；
  截图翻译时输出区绑定切到 `visionService.responseData`，
  状态机复用（Translating / Finished / Failed / Stopped）。
- 截图请求期间输入框可显示占位提示（如"正在识别截图内容…"），
  不把 base64 或图片塞进输入框。

---

## 6. CMake 变更

```cmake
find_package(Qt6 REQUIRED COMPONENTS Quick DBus)   # Linux Portal 需要
target_link_libraries(TransAI PRIVATE ... Qt6::DBus)
```

新增源文件加入 `CMakeLists.txt`；`tests/CMakeLists.txt` 增加
`tst_visionmodelcapability.cpp` 与 `tst_imagepayloadbuilder.cpp`（纯逻辑，无需显示服务器）。

---

## 7. 实施步骤（按可独立验收的里程碑排序）

### M1：捕获层（可独立冒烟）

- `ScreenshotCaptureService` + `PortalScreenshotBackend`（Linux）
  + QScreen 兜底。
- 临时调试入口（托盘菜单）验证 `imageCaptured` 能拿到图。
- 验收：Wayland 走 Portal 截图成功；取消发 `captureCanceled`；Portal 缺失发 `captureFailed`。

### M2：Vision 链路（核心）

- `ImagePayloadBuilder` + 单测（缩放、JPEG、透明底、空图）。
- `VisionTranslationService` + `VisionModelCapability` + 单测。
- QML：托盘/按钮触发 → 截图 → vision 请求 → 主窗口流式展示。
- 验收：用视觉模型完成一次截图翻译；文本翻译零回归。

### M3：设置页与持久化

- `Setting` 新键读写 + 默认值 + 旧配置兼容。
- SettingView 截图翻译卡片（模式/模型/快捷键/高级折叠）+ i18n。
- 模型能力提示与"跟随当前模型"逻辑。
- 验收：改配置重启后保持；旧 config.json 加载不丢字段。

### M4：快捷键与 CLI

- `--translate-screenshot`、单实例转发、托盘项、`screenshotShortCut` 注册
  （复用 GlobalShortcutManager 的第二路 action；headless 平台保持既有拒绝行为）。
- 验收：第二进程转发命令生效；X11 快捷键触发；Wayland 下有引导文案。

### M5（后续独立阶段，不在本期）：OCR 后备

- 按 `phase-2b` 补 `OcrService` / `ExternalProcessOcrService`，
  `screenshotTranslateMode=auto` 在 vision 不可用时降级 OCR。

---

## 8. 验收清单

### 8.1 功能

- [ ] 手动文本翻译、划词翻译零回归。
- [ ] 截图 → 视觉模型 → 流式结果展示在主窗口。
- [ ] 取消截图不报错；截图失败不崩溃且有提示。
- [ ] 模型不支持图片时给出"去设置页换视觉模型"的引导性错误。
- [ ] `screenshotVisionModel` 为空时跟随当前模型；非空时优先生效。
- [ ] 中断（abort）后发 `translationStopped`，无悬挂请求。

### 8.2 配置

- [ ] 旧 config.json 无新键时按默认值运行，保存后旧字段不丢。
- [ ] 设置页模型下拉复用模型检测；能力警告非阻断。
- [ ] 新字符串均有中英双语。

### 8.3 架构

- [ ] `TranslationService` / `PromptBuilder` / `StreamParser` 无截图或图片逻辑侵入。
- [ ] `ScreenshotCaptureService` 不依赖任何翻译模块。
- [ ] 图片压缩编码全部在 C++（`ImagePayloadBuilder`），QML 不碰 base64。

### 8.4 构建与测试

```bash
cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_PREFIX_PATH="$QT_PATH" ..
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/TransAI --background
```

---

## 9. 推荐提交拆分

```text
feat(capture): add screenshot capture service with portal backend
feat(vision): add image payload builder
feat(vision): add vision model capability heuristic
feat(vision): add vision translation service
feat(qml): wire screenshot translation into main window
feat(settings): add screenshot translation settings
feat(app): add --translate-screenshot command and hotkey
test(vision): cover payload builder and capability heuristic
```

---

## 10. 注意事项

1. 用户取消截图是正常路径，必须走 `captureCanceled` 而非错误。
2. 截图前隐藏主窗口，避免把自己截进去。
3. 启发式能力探测只做提示和路由，真正的门禁是服务端响应。
4. 不要把 vision payload 塞进 `PromptBuilder`；不要改动文本翻译的请求结构。
5. Wayland 下快捷键依赖用户在桌面环境绑定 CLI 命令，设置页文案必须说明。
6. 大截图必须先缩放再编码，避免 base64 体积爆炸（超长上下文 / 403）。

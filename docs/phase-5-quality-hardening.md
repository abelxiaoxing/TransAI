# Phase 5 质量加固方案（全量评审问题的修复计划与实施记录）

> 目标读者：负责实现的 coding agent
> 适用项目：TransAI / Qt6 / QML / C++17
> 前置条件：Phase 1–4 已完成（见 AGENTS.md）
> 来源：Phase 4 之后的全量代码评审（C++ + QML + 测试 + CI 全覆盖阅读）

本阶段不引入新功能，只做"把已知问题一次修干净"：正确性缺陷、架构债务、
性能、隐私、测试与 CI、文档漂移。每条都有明确的实现位置与验证方式。

---

## 0. 总览

| 编号 | 事项 | 章节 | 状态 |
|---|---|---|---|
| Q-1 | 历史记录写入的是流式结束时的输入框内容（与请求不一致） | §1.1 | ✅ |
| Q-2 | Ctrl/⌘+Enter 未 accepted，且在设置页也会触发翻译 | §1.2 | ✅ |
| Q-3 | 每次配置保存都写一次系统密钥环 | §1.3 | ✅ |
| Q-4 | tesseract 探测同步阻塞启动（最坏 3.5s） | §1.4 | ✅ |
| Q-5 | 模型探测的错误文案与翻译链路不一致 | §1.5 | ✅ |
| Q-6 | X11 选区复制用固定 300ms 猜测 | §1.6 | ✅ |
| Q-7 | AppView.qml 927 行单体；两个 service 接线逐行重复 | §2.1 | ✅ |
| Q-8 | QML 中硬编码调色板，绕过 ThemeColors | §2.2 | ✅ |
| Q-9 | 窗口几何常量在 QML/C++ 双写 | §2.3 | ✅ |
| Q-10 | configVersion 只写不读，无迁移钩子 | §2.4 | ✅ |
| Q-11 | TranslationHistory::removeAt 是死代码 | §2.5 | ✅ |
| Q-12 | provider 字符串字面量散落 | §2.6 | ✅ |
| Q-13 | 流式渲染每个 chunk 全量重排（O(n²)） | §3.1 | ✅ |
| Q-14 | 每次 append 复制整段响应字符串 | §3.2 | ✅ |
| Q-15 | 截图预览 data URL 常驻内存 | §3.3 | ✅ |
| Q-16 | 模型探测只有传输超时，无整体上限 | §3.4 | ✅ |
| Q-17 | 历史明文长期留存，无开关/无退出清理 | §4.1 | ✅ |
| Q-18 | API key 未归一化；密钥环状态 UI 不可见 | §4.2 | ✅ |
| Q-19 | 截图 OCR 历史丢失识别出的原文 | §4.3 | ✅ |
| Q-20 | 可点击文本无键盘焦点/无障碍语义 | §4.4 | ✅ |
| Q-21 | 高风险逻辑零测试（坐标映射/失焦策略/单实例/视觉请求体/密钥环抑制） | §5.1 | ✅ |
| Q-22 | I18n 键覆盖无自动化守护 | §5.2 | ✅ |
| Q-23 | CI 无 sanitizer，冒烟测试不校验 QML 错误 | §5.3 | ✅ |
| Q-24 | 文档/注释漂移；无格式化配置 | §6 | ✅ |

明确不做：qt_add_qml_module 迁移、全库 clang-format 重排、真实钥匙环后端
的自动化测试（无桌面会话时不可测）、qml lint 进 CI（本地无 Qt6 qmllint，
无法在提交前自证，留作后续）。理由记在对应章节末尾。

---

## 1. 正确性

### 1.1 Q-1 历史记录快照

`AppView` 目前把 `inputArea.text` 在 `onTranslationFinished` 里再读一次：
流式过程中用户改输入框、或 OCR 路径把识别文本写回输入框，都会让历史里的
source / targetLanguage 与实际请求不一致。

方案：`startTrans()` 与 OCR 分支在发请求前把 `requestSourceText` /
`requestTargetLanguage` 写进属性，`finishSession()` 只读快照。

### 1.2 Q-2 Ctrl/⌘+Enter

- 事件未 `accepted`，会继续冒泡；
- 设置页打开时同样触发 `startTrans()`，对隐藏的输入框发请求。

方案：仅在 `pageStack.currentIndex === 0 && mainWindow.visible` 时响应并
`event.accepted = true`。

### 1.3 Q-3 密钥环写入抑制

`updateConfig()` 每次都 `write()` + `read()` 回读校验；而它被模式切换、
语言切换、窗口几何、输入防抖反复调用。

方案：新增 `m_lastStoredApiKey` 缓存，只有密钥真正变化才写/删；同时把
"密钥是否在密钥环"暴露成 `apiKeyStoredSecurely` 通知属性供 UI 展示。

### 1.4 Q-4 OCR 可用性异步探测

`TesseractOcrEngine` 构造函数同步跑 `--version`(1.5s) + `--list-langs`(2s)，
发生在 `engine.load()` 之前。

方案：构造函数只**异步**启动探测，`available` 改为带 `NOTIFY` 的缓存值；
`recognize()` 在探测尚未完成时做一次同步兜底探测（只在用户刚启动就截图时发生）。

### 1.5 Q-5 统一网络错误映射

抽出 `src/providers/NetworkErrorMapper`，`StreamingChatClient` 与
`ModelDiscoveryService` 共用（服务端 payload 里的具体错误仍然优先）。

### 1.6 Q-6 X11 选区事件驱动

改为监听 `QClipboard::dataChanged`，300ms 定时器降级为兜底上限；正常情况下
几十毫秒即返回，慢应用也不会再漏。

---

## 2. 结构

### 2.1 Q-7 AppView 拆分与接线去重

新增 `qml/TranslationModeSelector.qml`（模式段选器）与
`qml/MainHistoryPopup.qml`（历史弹窗）；两个 service 的信号接线改为调用
AppView 上的具名函数（`beginSession` / `updateResultText` /
`showServiceError` / `finishSession` / `flushResultText`），每个 service
块从 ~25 行降到 ~6 行。

### 2.2 Q-8 ThemeColors 收口

新增 token：`onAccent`、`onAccentDisabled`、`borderIdle`、`hairline`、
`chipBg`、`chipBorder`、`controlDown`、`controlHover`、`controlIdle`、
`popupBorder`、`popupSelectedText`，替换 AppView / ModelSection /
RoundIconButton / SectionCard / StatusPill 中的字面量。

### 2.3 Q-9 窗口几何单源

`Setting` 暴露 `CONSTANT` 属性：`defaultWindowWidth/Height`、
`minWindowWidth/Height`、`maxWindowWidth/Height`；`main.qml` 与
`Setting::loadConfig()` 的钳制共用同一组常量。

### 2.4 Q-10 configVersion 迁移钩子

`loadConfig()` 读取 `configVersion`：`< 2` 走 `migrateLegacyConfig()`
（顶层 `apiServer/apiKey/model` → 分 provider 键），`> kConfigVersion`
打印警告。为将来 schema 变更留下明确入口。

### 2.5 Q-11 死代码

删除 `TranslationHistory::removeAt()`（无 UI 调用、无测试）。

### 2.6 Q-12 provider 常量

`ProviderDefaults::ProviderIds`（`openAi()` / `ollama()`）替换 C++ 侧
7 个文件中的字符串字面量；QML 侧仍与 `setting.provider` 的取值一致。

---

## 3. 性能

### 3.1 Q-13 流式渲染节流

`result.text` 改为 50ms 定时刷新（`pendingResultText` 暂存，结束时立即 flush），
避免每个 SSE chunk 触发一次 TextEdit 全量重排 + 滚动到底。

### 3.2 Q-14 响应拼接

新增 `appendResponseData()` 原地追加，替代 `setResponseData(m_responseData + chunk)`；
`setResponseData` 仍负责重置。

### 3.3 Q-15 预览内存

`grabbedScreenInfos()` 交出 data URL 后立即释放（`m_previewDataUrls` 置空），
`m_previewSizes` 保留供坐标映射。

### 3.4 Q-16 模型探测整体超时

`ModelDiscoveryService` 增加 30s 整体上限（与翻译链路的 5 分钟 cap 同思路），
超时记录明确文案后再 abort，由 finished 处理器发出终态。

---

## 4. 隐私与体验

### 4.1 Q-17 历史记录开关

`Setting` 新增 `historyEnabled`（默认 true）与 `clearHistoryOnExit`
（默认 false）；QML 新增 `qml/settings/HistorySection.qml` 卡片（记录开关、
退出清理开关、条目数、立即清空）；`main.cpp` 在 `aboutToQuit` 时按开关清理。

### 4.2 Q-18 密钥归一化与状态

`Setting::apiKey()` 返回 trim 后的值，写入密钥环/明文也使用 trim 后的值；
`ProviderSection` 在密钥下方显示"已保存在系统密钥环"提示。

### 4.3 Q-19 OCR 原文入历史

OCR 分支把识别文本作为 `requestSourceText`，历史条目同时保留截图标记与原文，
点击条目时回填输入框。

### 4.4 Q-20 无障碍

可点击的关闭/历史/复制/置顶/模式段选器/设置按钮补充
`Accessible.role` + `Accessible.name`（文案走 I18n）。

---

## 5. 测试与 CI

### 5.1 Q-21 新增测试

| 测试 | 覆盖 |
|---|---|
| `tst_networkerrormapper` | HTTP 错误码区间 → 用户文案 |
| `tst_screenshotgeometry` | preview 像素 → 原图矩形映射（含越界裁剪/退化输入） |
| `tst_windowdismiss` | 窗口内按下时间戳配对、offscreen 不自动隐藏 |
| `tst_applicationcommandserver` | 命令转发、陈旧 socket 接管 |
| `tst_streamingchatclient`（扩展） | vision 请求体（image_url + system prompt） |
| `tst_setting`（扩展） | history 键、窗口常量钳制、密钥环写入抑制、configVersion 迁移 |
| `tst_ocrengine`（扩展） | OCR 可用性异步探测（构造不阻塞 + 只通知一次） |

真实钥匙环后端（libsecret/Keychain/DPAPI）保持手工验证：CI 无桌面 keyring
daemon，强行测只会引入环境性 flake。

### 5.2 Q-22 I18n 键覆盖测试

`tst_i18n` 新增用例：扫描 `qml/**.qml` 里所有 `I18n.t("…")`，断言每个键都能
在 `_zhTable` 中找到；同时断言表里没有重复键。

### 5.3 Q-23 CI

- Linux 冒烟测试捕获 stderr，出现 QML `TypeError/ReferenceError/Unable to assign`
  等运行时错误即失败（当前只是"进程没死"）；
- 新增 sanitizer job（ASan + UBSan，`detect_leaks=0`）跑全部单测。

---

## 6. Q-24 文档与风格

- 新增本文件，并更新 `AGENTS.md`（Phase 2B/2C/2D 状态、目录树、新设置键、
  新测试、Phase 5 结论）；
- 修正 `Setting.h` 中"`ocr` 预留给后续阶段"的过期注释；
- `I18n.qml`：`"stop"` → `"Stop"`（统一首字母大写），补充新文案；
- 新增 `.clang-format`（Qt 风格基线），修正 `main.cpp` 缩进的 `#ifdef`、
  `Setting * setting` 这类明显不一致；
- 移除 `src/PropertyMacros.h` 的 UTF-8 BOM。

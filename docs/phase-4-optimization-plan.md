# Phase 4 优化方案：架构债务、体验与质量改进

> 目标读者：负责实现的 coding agent
> 适用项目：TransAI / Qt6 / QML / C++17
> 前置条件：已完成 Phase 1、2A、截图 vision 翻译、Phase 3（见 AGENTS.md）
> 来源：Phase 3 之后的全量代码评审（11 项优化点）

---

## 0. 总览

| 编号 | 事项 | 方案章节 | 本阶段状态 |
|---|---|---|---|
| O-1 | TranslationService / VisionTranslationService ~90% 重复 | §1 | ✅ 已实现 |
| O-2 | 只有无数据超时，无整体超时 | §1.4 | ✅ 已实现 |
| O-3 | 网络服务无 mock 测试 | §2 | ✅ 已实现（基类级） |
| O-4 | 无翻译历史 | §3 | ✅ 已实现 |
| O-5 | 目标语言无自动方向（中⇄英） | §4 | ✅ 已实现 |
| O-6 | 窗口尺寸不持久化 | §5 | ✅ 已实现 |
| O-7 | 错误消息不随语言切换重新翻译 | §6 | ✅ 已实现 |
| O-8 | Wayland 热键/点击外部引导不足 | §7 | ✅ 已实现（设置页提示增强） |
| O-9 | CI 只有 Linux | §8 | ✅ 已实现（Windows/macOS 阻断 job） |
| O-10 | API key 明文存储 | §9 | ✅ 已实现（原生后端，无新第三方依赖） |
| O-11 | OCR 截图翻译闭环（Phase 2B） | §10 | ✅ 已实现（tesseract CLI / macOS Vision） |
| O-12 | SettingView.qml 1556 行过大 | §11 | ✅ 已实现（拆为 10 个组件，骨架 384 行） |
| O-13 | QML↔C++ QVariantMap 弱类型 | §12 | ✅ 已实现（显式参数 Q_INVOKABLE） |
| O-14 | 输入防抖/实时翻译 | §13 | ❌ 明确不做（API 成本） |

延期项的决策理由统一记录在各章节末尾。

---

## 1. O-1/O-2：抽取 StreamingChatClient 基类 + 整体超时

### 1.1 现状问题

`TranslationService`（279 行）与 `VisionTranslationService`（340 行）逐行重复：
SSE 流式读取、30s 无数据传输超时、thinking 参数被拒后透明重试、
HTTP 错误码区间映射、单一终态信号模型。已有"修复只改一处"的前科
（`92705bb fix(translation): report transfer timeout as translationFailed`
只修了文本路径，vision 路径随后补修）。

### 1.2 设计

新增 `src/translation/StreamingChatClient.h/cpp`，作为两个服务的共同基类：

```cpp
class StreamingChatClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString responseData READ responseData NOTIFY responseDataChanged)
    Q_PROPERTY(QString responseError READ responseError NOTIFY responseErrorChanged)
    Q_PROPERTY(bool isRequesting READ isRequesting NOTIFY isRequestingChanged)

public:
    explicit StreamingChatClient(QObject* parent = nullptr);
    QString responseData() const;
    QString responseError() const;
    bool isRequesting() const;
    Q_INVOKABLE void abort();   // 用户停止：断开连接 → translationStopped

signals:
    // 与现有两个服务的信号形状完全一致，QML 侧零改动
    void responseDataChanged();
    void responseErrorChanged();
    void isRequestingChanged();
    void translationStarted();
    void translationFinished(QString result);
    void translationFailed(QString error);
    void translationStopped();

protected:
    // 子类组装好的一个可发送请求。bodyWithoutThinking 是去掉 thinking 参数
    // 后的等价请求体（无 thinking 参数时与 body 相等）；二者不相等才允许
    // "被拒后透明重试"。
    struct PendingRequest {
        QNetworkRequest networkRequest;
        QJsonObject body;
        QJsonObject bodyWithoutThinking;
    };

    // 子类在校验通过后调用：取消旧请求、清空状态、发信号、发出请求。
    void startRequest(const PendingRequest& request);
    // 校验失败路径：记录错误并发出终态信号（不发 started）。
    void failValidation(const QString& message);
    // 子类可重写：失败消息发出前的最后加工（vision 用它把
    // "image not supported" 原始错误改写成引导文案）。
    virtual QString decorateFailureMessage(const QString& error) const;

private:
    void cancelCurrentRequest();
    void handleStreamData(QNetworkReply* reply);
    void sendPendingRequest();
    // ... responseData/responseError/isRequesting setter 与错误映射
};
```

要点：

- **信号/Q_PROPERTY 形状不变**：QML（`AppView.qml` 的两个服务块）与
  `main.cpp` 的 `qmlRegisterType` 完全不动。
- **重试逻辑下沉**：finished 处理器中，若 `m_responseData` 为空、错误命中
  `ThinkingPolicyBuilder::isThinkingParamRejection` 且
  `bodyWithoutThinking != body`，则换用 `bodyWithoutThinking` 重发一次。
  重试预算（每次用户请求一次）由基类的 `m_thinkingRetryUsed` 管理，
  `startRequest()` 时重置。
- **终态信号唯一**：与现状相同——每次请求恰好发出
  finished/failed/stopped 之一。
- `TranslationService` 只保留：参数校验、`ThinkingPolicyBuilder` + `PromptBuilder`
  组装 body、构造 `PendingRequest`。
- `VisionTranslationService` 只保留：参数校验、`ImagePayloadBuilder` 调用、
  vision messages 组装（system prompt + data-url image content）、
  `decorateFailureMessage()` 重写（vision 不支持引导）。

### 1.3 子类划分原则

- 基类不知道"翻译"这回事：不引入 `TranslationRequest` / `VisionRequest`，
  只认 `PendingRequest`。请求 struct 留在各自子类。
- `networkErrorMessage()`（HTTP 错误码区间映射）下沉为基类私有静态方法。

### 1.4 O-2：整体超时

现状只有 `QNetworkAccessManager::setTransferTimeout(30s)`（无数据超时，
有数据就重置）。慢速本地模型持续缓慢吐 token 时会无限占用会话。

基类增加 `QTimer m_overallTimeout`（single-shot，常量
`kOverallTimeoutMs = 5 * 60 * 1000`）：

- `sendPendingRequest()` 时启动；finished/取消时停止。
- 触发时：记录 `m_responseError = "Translation timed out: the server took
  too long to respond"`，然后 `reply->abort()`（**不断开连接**）——
  finished 处理器会收到 `OperationCanceledError`，沿用现有
  "非空 responseError 优先" 逻辑发出 `translationFailed`。
  用户主动 `abort()` 走 `cancelCurrentRequest()`（先断开再 abort），
  两条路径不会混淆。

### 1.5 验收

- 全量单测通过；offscreen 冒烟通过。
- QML 行为零回归：流式翻译 / 停止 / 截图翻译 / 划词翻译。
- 新增 mock 测试覆盖基类（见 §2）。

---

## 2. O-3：网络服务的 mock endpoint 测试

新增 `tests/tst_streamingchatclient.cpp`：

- 内嵌一个基于 `QTcpServer` 的迷你 HTTP fixture（~120 行）：读请求头与
  body（按 `Content-Length`），按脚本回复——SSE 流（若干
  `data:` 行 + `[DONE]`）、或裸 JSON 错误体 + HTTP 状态码。
  记录每次收到的请求 body 供断言。
- 直接测试 `TranslationService`（provider=`ollama` 免 apiKey 校验，
  apiServer 指向 `http://127.0.0.1:<port>`；ollama 路径的 thinking 参数
  由模型 id 触发，用 `qwen3-thinking` 之类 id 命中 `think:false`）：
  1. 正常 SSE 流 → `translationFinished`，内容拼接正确；
  2. 首次 400 "Unknown parameter 'think'" → 自动重试且第二次请求体
     **不含** `think` 字段 → 流式成功；
  3. HTTP 500 → `translationFailed` 且消息为服务端错误映射；
  4. 请求中 `abort()` → `translationStopped`，无 finished/failed。
- 全部用 `QSignalSpy` + `QTRY_VERIFY`，超时裕量 5s，避免时序 flaky。

---

## 3. O-4：翻译历史

### 3.1 C++：`src/app/TranslationHistory.h/cpp`

- 上下文属性单例 `translationHistory`（与 `setting`/`clipboard` 同模式）。
- API：
  - `Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)`，
    最新在前；每条 map：`{type: "text"|"screenshot", source, result,
    targetLanguage, timestamp}`（ISO8601 字符串）。
  - `Q_INVOKABLE void addEntry(const QString& type, const QString& source,
    const QString& result, const QString& targetLanguage)`
  - `Q_INVOKABLE void clear()`
  - `Q_INVOKABLE void removeAt(int index)`（预留，本阶段 UI 不用）
- 持久化：`AppConfigLocation/translation-history.json`，`QSaveFile` 原子写，
  Unix 上 chmod 600（与 config.json 一致）。上限 100 条，超出丢弃最旧。
  空结果/空白输入不记录。
- 加载失败（JSON 损坏）：备份为 `.invalid.bak` 并置空，与 config 同策略。

### 3.2 QML（`AppView.qml`）

- 两个服务的 `onTranslationFinished` 中调用 `translationHistory.addEntry(...)`
  （文本：`type:"text", source:inputArea.text`；截图：`type:"screenshot",
  source:""`）。
- header 的设置按钮左侧加一个文本小按钮 "History"（样式同 ✕）→ 打开
  `historyPopup`（`Popup`）：
  - `ListView` 展示条目：来源片段（截图条目显示本地化 "[Screenshot]"）、
    结果片段、相对友好宽度；
  - 点击条目 → 回填 `inputArea.text` / `result.text` 并关闭；
  - 底部 "Clear" 按钮；空态显示 "No history yet"。
- i18n 新增：`"History"→"历史记录"`、`"Clear"→"清空"`、
  `"No history yet"→"暂无历史记录"`、`"[Screenshot]"→"[截图]"`。

### 3.3 边界

- 历史只记录成功完成（finished）的结果；停止/失败不记录。
- 截图历史不存图片本身（体积），只存识别+翻译文本。
- 历史面板打开时不应触发"点击窗口外隐藏"误判：Popup 默认
  `Popup.Item` 在窗口场景内，已有 outsideClickCheckTimer 机制覆盖。

---

## 4. O-5：目标语言自动方向

### 4.1 设计

- `AppView.qml` 的 `langSelector` model 首项插入本地化 `"Auto"`。
- 持久化哨兵值：`setting.targetLanguage = "__auto__"`（永不本地化，
  不受界面语言切换影响）；恢复时 `"__auto__"` → index 0。
- 解析规则（`AppView.effectiveTargetLang(sourceText)`）：
  源码含 CJK 字符（含日文假名/韩文音节）→ `"English"`；否则 → `"简体中文"`。
  - 正则：`/[\u3400-\u9fff\uf900-\ufaff\u3040-\u30ff\uac00-\ud7af]/`
  - 非 Auto 时直接返回 `langSelector.currentText`。
- `startTrans()` 与 `startScreenshotTranslation()` 都改用该函数
  （截图 OCR 出的文本方向同样适用；截图场景源文本不可见，用 Auto 时
  按目标语言默认 `"简体中文"`？——不，截图无法预知文本语言，Auto 对
  截图固定解析为 `"English"` 不合理。决策：**截图路径沿用 Auto 解析但
  以 UI 语言为基准**：中文 UI → 翻译成简体中文；英文 UI → English。
  文本路径以源文本脚本为准。两个解析函数分开，注释说明。）

> 实现备注：截图路径的 Auto 解析为 `I18n.isChinese ? "简体中文" : "English"`；
> 文本路径按脚本检测。config 迁移：旧值（具体语言名）不受影响；
> 非法值回落 index 0 即 Auto（行为变化：旧默认"简体中文"变为 Auto。
> Auto 对非 CJK 输入的解析结果就是简体中文，行为兼容）。

### 4.2 验收

- Auto + 中文输入 → 请求体目标语言为 English；Auto + 英文输入 → 简体中文。
- 选择 Auto 重启后仍保持 Auto；切换界面语言 Auto 文案跟随、选择不丢。

---

## 5. O-6：窗口尺寸持久化

- Setting 新增 `windowWidth` / `windowHeight`（int，0=未设置）。
  `loadConfig()` 钳制到 `[400, 3840]` / `[500, 2160]`（与 main.qml 的
  minimumWidth/Height 对齐），越界归 0。
- `main.qml`：
  - `Component.onCompleted` 中若设置值有效则**命令式**赋值
    （不用绑定——用户拖拽调整窗口尺寸由 WM 改写 width/height，
    绑定会被打断或打架）。
  - `onWidthChanged`/`onHeightChanged` → 1s 防抖 Timer → 写 Setting +
    `updateConfig()`。跳过未初始化完成前的程序化赋值（用标志位）。
- 不持久化 x/y：Wayland 客户端无法自定位；现有居中 + `ensureWindowVisible()`
  行为保留。

---

## 6. O-7：错误消息随语言切换重新翻译

- `AppView.qml` 新增 `property string lastRawError: ""` 与
  `property bool errorDisplayed: false`。
- 四个错误显示点（两个 `onResponseErrorChanged`、`showScreenshotError`、
  `showSelectionError`）记录原文并置位；`onTranslationStarted` 清除。
- 新增 `Connections { target: I18n; function onLanguageChanged() {...} }`：
  若 `errorDisplayed && !statusBusy`，用 `localizeError(lastRawError)`
  重设 `result.text`。
- 已知限制收敛为：仅"当前仍显示着的错误"会重译；历史遗留文本不重译。

---

## 7. O-8：Wayland 引导增强（设置页）

`SettingView.qml` 的热键卡片（现有两处 Wayland 提示文案）：

1. 提示文案下方加 "Copy command" 小按钮（复用 `clipboard` 上下文属性 +
   "Copied ✓" 反馈模式），分别复制 `TransAI --translate-selection` /
   `TransAI --translate-screenshot`。
2. 当 `!hotkeysSupported`（Wayland）且 `globalClickMonitor.available === false`
   时，追加一条提示：点击外部隐藏不可用，需 /dev/input 读取权限
   （指向 docs/hyprland-integration.md）。
3. i18n 新增：`"Copy command"→"复制命令"`、点击外部提示词条。

---

## 8. O-9：Windows / macOS CI

`ci.yml` 新增两个 job（均为阻断门禁）：

```yaml
windows:
  runs-on: windows-latest
  steps: checkout → install-qt-action (6.9.*, archives qtbase qtdeclarative,
         tools tools_mingw1310) → 优先使用 Qt 自带 MinGW → cmake -B build
       → cmake --build build → ctest
macos: # 同构，macos-latest
```

- 只构建 + 单测，不做 GUI 冒烟（无显示/权限环境）。
- **已转为阻断**（原设计的 `continue-on-error: true` 已移除）：三平台在
  首次接入后即全绿（Linux/macOS/Windows 各 10/10 单测），“跑绿数周
  再转强制”的观察期不再必要。
- 接入过程中 CI 确实抓到了真问题，验证了这两个 job 的价值：
  `SecretStoreMac.mm` / `VisionOcrEngine.mm` 缺 `Foundation` 导入、
  `SecretStoreWindows.cpp` 的 const 限定符、以及 Windows 上用 Qt 自带
  MinGW 而非 runner 系统 gcc（`__imp___argc` 链接失败）。

---

## 9. O-10：API key 安全存储（✅ 已实现）

### 方案

- 引入系统密钥环抽象 `src/settings/SecretStore.h`（接口：
  `read(key)/write(key,value)`，同步包装异步 API 或直接用同步原生 API）：
  - Linux：`org.freedesktop.secrets`（libsecret，D-Bus）；
  - Windows：DPAPI `CryptProtectMemory`→ 文件存密文，或 wincred；
  - macOS：Keychain Services（Security framework）。
  - 推荐直接 vendored **QtKeychain**（`lib/qtkeychain`，与 QHotkey 同模式），
    三平台已封装好。
- `Setting`：`openaiApiKey` 的读写改走 SecretStore；config.json 中
  `openaiApiKey` key 迁移为 `openaiApiKeyInKeychain: true` 标记。
  启动时若 config 有明文 key 且 keychain 为空 → 写入 keychain 并
  从 config 抹除（一次性迁移，下次保存生效）。
- 降级：密钥环不可用（headless Linux 无 D-Bus）→ 回退明文 + 日志警告。

### 实现备注（O-10，替代原 QtKeychain 方案）

- 未引入 QtKeychain，改为极薄的原生后端，每平台一个文件、仅一个后端被
  编译（`createPlatformSecretStore()` 由该文件提供）：
  - Linux：`SecretStoreLinux.cpp`（libsecret，CMake `pkg-config`
    检测存在才编译，定义 `TRANSAI_HAVE_LIBSECRET`）；该编译单元加
    `QT_NO_KEYWORDS`（glib 头以 `signals` 作成员名与 Qt 宏冲突）。
  - macOS：`SecretStoreMac.mm`（Security framework，generic password）。
  - Windows：`SecretStoreWindows.cpp`（DPAPI `CryptProtectData`，密文
    base64 存 `secrets.dat`）。
  - 其它/无 libsecret：`SecretStoreNull.cpp` 返回 nullptr → 明文回退。
- 迁移与回退保证（`Setting`）：密钥环优先读；明文仅在“写入密钥环 +
  回读一致”后才从 config.json 抹除；任何失败保持原明文行为。
  `openaiApiKeyInSecretStore: true` 为信息性标记。
- `Setting` 第二构造函数注入 SecretStore（测试缝）；`tst_setting` 用
  内存 FakeSecretStore 覆盖：store 优先、迁移抹除、保存写入、不可用
  回退、清空删除 5 个用例。
- Linux 无 Secret Service（headless）时 `isAvailable()` 探测失败即回退，
  仅一条 qInfo 日志。

### 遗留注意

- macOS/Windows 后端按 API 文档编写但未在真机编译验证（本环境仅
  Linux）；首个 Win/macOS CI job（§8）会覆盖编译，运行期行为需真机
  确认。回退保证确保任何后端失败都不会丢失已有 key。

---

## 10. O-11：OCR 截图翻译闭环（✅ 已实现，即 Phase 2B）

### 方案要点（细化自 docs/phase-2b）

- `screenshotTranslateMode` 已有 `"auto"|"vision"|"ocr"` 三态预留。
- 新增 `src/ocr/`：
  - `OcrEngine.h`（接口：`recognize(QImage) → QString`）；
  - Linux：Tesseract（`tesseract-ocr` 命令行包装，避免链接 C API；
    检测 `PATH` 中是否存在，缺失则 OCR 模式提示安装）；
  - macOS：Vision framework `VNRecognizeTextRequest`（系统自带）；
  - Windows：Windows.Media.Ocr（WinRT）或同走 Tesseract。
- 闭环：截图 → OCR 文本 → 走 `TranslationService` 文本路径（复用
  PromptBuilder/模式选择），结果面板照常；`auto` 模式 =
  当前模型是 vision → vision 路径，否则 OCR 路径。
- UI：截图卡片加模式选择（Auto / Vision / OCR），OCR 不可用时禁用并提示。

### 实现备注（O-11）

- `src/ocr/OcrEngine.h`：异步接口（`recognize(QImage)` → `recognized /
  recognitionFailed`，`cancel()`），`available` 属性；QML 通过上下文
  属性 `ocrEngine` 访问（null 后端也返回有效对象，QML 无需空值守卫）。
- Linux/Windows：`TesseractOcrEngine` 走 CLI（`tesseract stdin stdout`，
  PNG 经 stdin 传入），不链接 C API——tesseract 缺失仅导致 OCR 模式
  不可用并在设置页提示；`--list-langs` 探测语言包，优先 `chi_sim+eng`
  （`OcrLanguageSelection` 纯函数有单测）。
- macOS：`VisionOcrEngine.mm`（`VNRecognizeTextRequest`，accurate，
  zh-Hans+en，后台队列执行 + QueuedConnection 回主线程）。
- 路由（`AppView.effectiveScreenshotMode()`）：`vision`/`ocr` 显式优先；
  `auto` = 视觉模型启发式命中 → vision，否则 OCR 可用 → OCR，兜底 vision。
  OCR 识别出的文本进输入框并走文本翻译管线（mode 强制 Translate）。
- UI：截图卡片新增 MODE 下拉（Auto / Vision model / OCR），OCR 不可用
  时选择 OCR 显示安装提示；OCR 进行中状态 pill 显示“正在识别文字…”，
  Stop 按钮同时取消 OCR。OCR 会话历史记为 screenshot 类型。

### 延期理由（已被上述实现取代，保留备查）

- 三平台 OCR 引擎接入工作量大（各 1 天+），且 Tesseract 中文包、
  WinRT 头等均需真机验证；vision 路径已覆盖主场景（云端模型普遍
  支持图像）。Phase 2B 蓝图已存在，可作为独立迭代。

---

## 11. O-12：SettingView.qml 拆分（✅ 已实现）

目标拆分（纯机械移动，零行为变化）：

```
qml/settings/
├── ProviderSection.qml    # AI Provider 卡片（server/key/provider）
├── ModelSection.qml       # 模型检测/选择卡片
├── HotkeySection.qml      # 全局热键卡片（含 Wayland 提示）
├── ScreenshotSection.qml  # 截图翻译卡片（vision model/高级选项）
├── LanguageSection.qml    # 界面语言卡片
└── SectionCard.qml        # 卡片容器（标题+圆角背景，现有重复样式下沉）
```

- `SettingView.qml` 只保留页面骨架与各 Section 的装配/属性接线
  （`hotkeyError`、`shortcutRegistrar` 等通过属性注入）。
- 每个 Section 文件 ≤300 行。
- 验收：offscreen 冒烟 + 手动过一遍设置页所有交互。

### 实现备注（与设计的偏差）

- 共享组件实际抽出 5 个：SectionCard / SettingsRow / StatusPill /
  RoundIconButton / ShortcutRecorder（ ShortcutRecorder 顺带统一了
  Global Hotkey 卡片原来的手写录制器，行为等价）。
- `qmldir` 增加 `ThemedComboBox 1.0` 条目：`qml/settings/` 组件通过
  `import ".."` 使用它与单例（解析到同一模块 URL，单例实例共享）。
- `ModelSection` 拥有 `ModelDiscoveryService` 并暴露 `detector` 只读
  属性；截图卡片的 vision 模型列表与头部状态 pill 经它取数。
- main.qml 面向 SettingView 的 API（reload/flushPendingSave/
  hotkeyError/shortcutRegistrar/hotkeysSupported）零变化。
- 拆分后规模：SettingView 384 行；最大的 ModelSection 381 行。

---

## 12. O-13：QVariantMap → 显式参数 API（✅ 已实现）

- `TranslationRequest` 改为 `Q_GADGET`（`Q_PROPERTY` + `Q_INVOKABLE` 构造），
  QML 侧：
  ```qml
  var req = TranslationRequestComponent.create(text, ...)  // 或 property 赋值
  translationService.translate(req)
  ```
- `TranslationService::translate(const TranslationRequest&)` 提升为
  `Q_INVOKABLE`；`translateFromMap` 保留一个版本周期标 `Q_DECL_DEPRECATED`
  后删除。
- vision 路径的 options map 同理定义 `VisionRequestOptions` gadget。

### 实现备注（O-13，替代原 Q_GADGET 方案）

- 未引入 `Q_GADGET`/`QML_VALUE_TYPE` 注册（项目用 `qmlRegisterType` +
  qrc，不走 `qt_add_qml_module`，gadget 值类型注册代价大收益小）。
- 改为**显式参数 Q_INVOKABLE**：
  `TranslationService.translate(text, targetLanguage, mode, provider,
  apiServer, apiKey, model)` 与
  `VisionTranslationService.translateImage(image, targetLanguage,
  provider, apiServer, apiKey, model, maxImageWidth, jpegQuality)`。
  arity/类型错误在调用点即报（QML 运行时 TypeError），不再有 map key
  拼写静默失败。
- `translateFromMap` / `translationRequestFromMap` 已删除；
  `TranslationRequest` 保留为纯 C++ struct（`translationModeFromInt`
  仍在）。AppView 与 mock 测试均已迁移到显式调用。

（原 Q_GADGET 设计保留备查；若未来迁移到 qt_add_qml_module 可再评估。）

---

## 13. O-14：输入防抖实时翻译（明确不做）

- 理由：每次输入都消耗 API 配额（云端按 token 计费）；本地 Ollama
  会阻塞 GPU。与"快速工具"定位不符。
- 已部分覆盖需求：Ctrl+Enter / 按钮触发足够轻量；划词翻译自动填充+触发。

---

## 14. 改动文件清单（本轮已实现部分）

| 文件 | 改动 | 章节 |
|---|---|---|
| `src/translation/StreamingChatClient.h/cpp` | 新增基类（含整体超时） | §1 |
| `src/translation/TranslationService.h/cpp` | 改为子类 + 显式参数 API | §1/§12 |
| `src/vision/VisionTranslationService.h/cpp` | 同上 + decorateFailureMessage | §1/§12 |
| `src/translation/TranslationRequest.h/cpp` | 删除 QVariantMap 转换 | §12 |
| `tests/tst_streamingchatclient.cpp`、`tests/CMakeLists.txt` | mock SSE 测试 | §2 |
| `src/app/TranslationHistory.h/cpp` | 新增历史服务 | §3 |
| `src/main.cpp` | 注册 `translationHistory`/`ocrEngine` 上下文属性 | §3/§10 |
| `qml/AppView.qml` | 历史按钮+弹窗、Auto 语言、错误重译、OCR 路由与状态、finished 记录历史 | §3/§4/§6/§10 |
| `src/settings/Setting.h/cpp` | `windowWidth/windowHeight`、SecretStore 集成 | §5/§9 |
| `src/settings/SecretStore*.h/cpp/mm`（4 平台文件） | API key 密钥环后端 | §9 |
| `src/ocr/OcrEngine.h` + 3 个后端文件 | OCR 引擎抽象与实现 | §10 |
| `tests/tst_setting.cpp`、`tests/tst_ocrengine.cpp` | 密钥环迁移（fake store）、OCR 语言选择 | §9/§10 |
| `qml/main.qml` | 窗口尺寸恢复与防抖保存 | §5 |
| `qml/SettingView.qml` + `qml/settings/`（10 个组件） | 设置页拆分、Wayland 提示、截图 MODE 下拉 | §7/§10/§11 |
| `qml/I18n.qml` | 新增词条 | §3/§4/§7/§10 |
| `.github/workflows/ci.yml` | Windows/macOS 阻断 job（含 Qt 自带 MinGW）、libsecret-1-dev | §8/§9 |
| `CMakeLists.txt` | 新源文件、平台后端接线 | — |
| `AGENTS.md` | 结构/配置 key/测试说明更新 | — |

## 15. 测试策略

- `ctest`：既有 7 个 + 新增 tst_streamingchatclient + tst_translationhistory。
- 冒烟：offscreen 双进程命令转发。
- 手动回归清单：流式翻译（OpenAI + Ollama）、停止、截图翻译、划词、
  Auto 语言两个方向、历史回填/清空、设置页 Wayland 提示
  （无 /dev/input 权限时）、窗口尺寸重启保持、错误显示后切换语言重译。

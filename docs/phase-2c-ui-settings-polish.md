# Phase 2C 实施任务书：Popup、设置页与体验增强

> 目标读者：负责实现 Phase 2C 的 coding agent  
> 适用项目：TransAI / Qt6 / QML / C++17  
> 前置条件：建议已完成 Phase 2A 和 Phase 2B

---

## 0. 执行前必读

实现前请先阅读：

- `AGENTS.md`
- `docs/phase-1-architecture-refactor.md`
- `docs/phase-2a-selection-translation.md`
- `docs/phase-2b-screenshot-ocr-translation.md`
- `src/app/TranslationCoordinator.h/cpp`
- `src/translation/TranslationService.h/cpp`
- `qml/main.qml`
- `qml/AppView.qml`
- `qml/SettingView.qml`
- `CMakeLists.txt`

如果划词翻译或 OCR 翻译闭环尚未完成，不建议先做本阶段。

---

## 1. 本阶段目标

在不改变底层翻译管线的前提下，增强用户体验：

```text
TranslationPopup
WaylandCapabilityStatus
ScreenshotPreview
SettingView 新增基础配置项
```

本阶段只做 UI 与设置增强，不实现新的底层能力。

---

## 2. 非目标

本阶段不要实现：

- 不实现 Portal Shortcut。
- 不实现 RemoteDesktop Portal。
- 不实现 VisionTranslationService。
- 不重写整个 AppView。
- 不改动 `TranslationService` 核心逻辑。
- 不把翻译业务流程塞进 QML。

---

## 3. TranslationPopup

### 3.1 新增文件

```text
qml/TranslationPopup.qml
```

### 3.2 目标

用于划词翻译或截图 OCR 翻译结果的轻量展示。

### 3.3 建议能力

- 显示原文。
- 显示翻译结果。
- 支持流式更新。
- 支持复制结果。
- 支持关闭。
- 支持 Escape 关闭。
- 支持 pin，pin 后不自动关闭。
- 自动避开屏幕边缘。
- 复用现有主题颜色。

### 3.4 不应该做的事

`TranslationPopup.qml` 不应该：

- 直接读取剪贴板。
- 直接注册快捷键。
- 直接构造完整翻译 request。
- 直接处理 OCR。
- 直接访问 Provider 底层逻辑。

Popup 只展示状态。

---

## 4. TranslationCoordinator 与 Popup 连接

如果 Phase 2A 中已有信号：

```cpp
void requestShowPopup(QString sourceText, QString translatedText, QPoint position);
```

本阶段可以完善。

如果没有，可新增更适合流式展示的信号：

```cpp
signals:
    void requestShowPopup(QString sourceText, QPoint position);
    void popupTranslationUpdated(QString translatedText);
    void popupTranslationFinished(QString translatedText);
    void popupTranslationFailed(QString reason);
```

要求：

- Coordinator 只发信号，不直接操作 QML 对象。
- QML 负责创建 / 显示 / 关闭 Popup。
- Popup 可以绑定 `TranslationService.responseData`，但不要发起翻译流程。

---

## 5. ScreenshotPreview

### 5.1 新增文件

```text
qml/ScreenshotPreview.qml
```

### 5.2 目标

用于截图 OCR 前后的确认和预览。

建议展示：

- 截图缩略图。
- OCR 识别文本。
- “翻译”按钮。
- “复制 OCR 文本”按钮。
- “取消”按钮。

### 5.3 注意

这是体验增强，不是 OCR 闭环必须项。

如果实现复杂度较高，可以只实现最小预览，或后置。

---

## 6. WaylandCapabilityStatus

### 6.1 新增文件

```text
qml/WaylandCapabilityStatus.qml
```

### 6.2 目标

显示当前环境能力，例如：

```text
Platform: wayland / xcb / windows / cocoa
Shortcut backend: QHotkey / Portal / Fallback / Unavailable
Selection mode: Primary Selection / Clipboard / Manual Copy
Screenshot portal: Available / Unavailable / Unknown
OCR backend: tesseract available / missing
```

### 6.3 要求

- 状态不可用时不报错。
- 未实现 Portal 检测时显示 Unknown，而不是误报 Available。
- 信息应帮助用户理解为什么某些功能不可用。

---

## 7. SettingView 扩展

### 7.1 建议新增分组

```text
Global Shortcuts
 ├─ Translate Selection
 ├─ Screenshot Translate
 └─ Backend Status

Wayland Integration
 ├─ Shortcut Backend: Auto / Portal / QHotkey / Disabled
 ├─ Selection Mode: Auto / Primary Selection / Clipboard Only / Manual Copy
 └─ Screenshot Portal: Enabled / Disabled

Image Translation
 ├─ Mode: OCR then Translate
 ├─ OCR Backend
 ├─ OCR Language
 └─ OCR Command
```

### 7.2 配置建议

新增配置：

```json
{
  "shortcuts": {
    "translateSelection": "Ctrl+Shift+T",
    "translateScreenshot": "Ctrl+Shift+S",
    "showMainWindow": ""
  },
  "wayland": {
    "shortcutBackend": "auto",
    "selectionMode": "auto",
    "useRemoteDesktopPortal": false,
    "useScreenshotPortal": true
  },
  "ocr": {
    "backend": "externalTesseract",
    "language": "eng+chi_sim",
    "command": "tesseract"
  },
  "imageTranslation": {
    "mode": "ocrThenTranslate",
    "visionModel": "",
    "maxImageWidth": 1600,
    "jpegQuality": 85
  }
}
```

### 7.3 旧配置兼容

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

兼容策略：

- `shortcuts.translateSelection` 不存在时，使用旧 `shortCut`。
- 保存时可以同时写新旧字段。
- 不要删除旧字段。
- 不要改变配置文件路径。

---

## 8. 主题与交互一致性

新 QML 组件应遵守现有主题系统：

- 使用已有颜色变量或 ThemeController。
- 保持暗色主题一致。
- 不引入完全独立的视觉风格。
- Popup 应避免遮挡过多屏幕内容。

---

## 9. 验收清单

### 9.1 Popup 验收

- [ ] `TranslationPopup.qml` 可以显示原文。
- [ ] 可以显示流式翻译结果或最终结果。
- [ ] 可以复制翻译结果。
- [ ] 可以关闭。
- [ ] Escape 可关闭。
- [ ] 不直接发起翻译业务流程。
- [ ] 主题风格与应用一致。

### 9.2 设置页验收

- [ ] 新增快捷键配置展示。
- [ ] 新增 OCR command / language 配置展示。
- [ ] 新增 Wayland 相关基础配置展示。
- [ ] 旧配置文件可正常读取。
- [ ] 旧字段不丢失。
- [ ] 保存配置不破坏 Provider 设置。

### 9.3 状态展示验收

- [ ] 能显示当前 platform。
- [ ] 能显示快捷键 backend 状态，或 Unknown。
- [ ] 能显示 OCR backend 状态，或 missing。
- [ ] 能解释 fallback 状态。

### 9.4 构建验收

至少执行：

```bash
cmake --build build -j$(nproc)
```

---

## 10. 推荐提交拆分

```text
feat(ui): add translation popup
feat(app): expose popup display signals
feat(ui): add screenshot preview
feat(ui): add Wayland capability status view
feat(settings): add shortcut settings
feat(settings): add OCR settings
feat(settings): preserve legacy config fields
```

---

## 11. 注意事项

1. 本阶段是体验增强，不要改底层翻译管线。
2. 不要让 QML 接管复杂业务流程。
3. 不要删除旧配置字段。
4. 不要把多模态设置做成默认可用，除非后续 VisionTranslationService 已实现。
5. 未实现的能力应显示 Unknown / Unavailable，不要误导用户。

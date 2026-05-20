# Phase 2B 实施任务书：截图 / 图片 OCR 翻译闭环

> 目标读者：负责实现 Phase 2B 的 coding agent  
> 适用项目：TransAI / Qt6 / QML / C++17  
> 前置条件：已完成 Phase 1，建议已完成 `docs/phase-2a-selection-translation.md`

---

## 0. 执行前必读

实现前请先阅读：

- `AGENTS.md`
- `docs/phase-1-architecture-refactor.md`
- `docs/phase-2a-selection-translation.md`
- `src/translation/TranslationRequest.h`
- `src/translation/TranslationService.h/cpp`
- `src/app/TranslationCoordinator.h/cpp`
- `qml/main.qml`
- `qml/AppView.qml`
- `qml/SettingView.qml`
- `CMakeLists.txt`

如果 `TranslationCoordinator` 尚不存在，请先按 Phase 2A 创建最小版本。

---

## 1. 本阶段目标

实现截图 / 图片 OCR 翻译闭环：

```text
截图或选择本地图片
    ↓
QImage
    ↓
OcrService / ExternalProcessOcrService
    ↓
recognized text
    ↓
TranslationRequest(source = ScreenshotOcr)
    ↓
TranslationService
    ↓
主窗口展示结果
```

本阶段优先实现：

```text
图片 / 截图 → OCR → 文本翻译
```

不实现多模态图片翻译。

---

## 2. 非目标

本阶段不要实现：

- 不实现 `VisionTranslationService`。
- 不实现 OpenAI vision payload。
- 不实现完整 Popup 体验。
- 不重写设置页。
- 不把 OCR 逻辑塞进 `TranslationService`。
- 不把截图逻辑塞进 `PromptBuilder`。
- 不强制依赖 Portal Screenshot 才能完成闭环。

如果 Portal Screenshot 实现成本较高，可以先完成本地图片输入 OCR 闭环，再补 Portal。

---

## 3. Wayland 截图约束

Wayland 下不应优先依赖：

```cpp
QScreen::grabWindow(0)
```

因为它通常无法可靠截取其他应用内容。

推荐方案：

```text
xdg-desktop-portal Screenshot
```

但 Portal 接入需要 D-Bus，且不同环境行为不同。因此本阶段允许两步走：

```text
1. 先实现 ImageInputService，支持选择本地图片完成 OCR 闭环
2. 再实现 PortalScreenshotBackend，支持交互式截图
```

---

## 4. ScreenshotCaptureService

### 4.1 新增文件

```text
src/capture/ScreenshotCaptureService.h
src/capture/ScreenshotCaptureService.cpp
src/capture/PortalScreenshotBackend.h
src/capture/PortalScreenshotBackend.cpp
```

### 4.2 推荐接口

```cpp
class ScreenshotCaptureService : public QObject
{
    Q_OBJECT

public:
    explicit ScreenshotCaptureService(QObject* parent = nullptr);

    Q_INVOKABLE void captureInteractive();

signals:
    void imageCaptured(QImage image);
    void captureCanceled();
    void captureFailed(QString reason);
};
```

### 4.3 Portal 流程

```text
captureInteractive()
    ↓
调用 org.freedesktop.portal.Screenshot
    ↓
用户通过系统 UI 选择截图区域或确认截图
    ↓
Portal 返回文件 URI
    ↓
加载 QImage
    ↓
imageCaptured(image)
```

### 4.4 要求

- 截图失败不崩溃。
- 用户取消截图应发 `captureCanceled()`。
- Wayland 优先走 Portal。
- 非 Wayland 可后续使用 QScreen fallback。
- 如果本阶段暂不实现 D-Bus Portal，必须给出明确 `captureFailed()`，并保证本地图片输入路径可用。

---

## 5. ImageInputService

### 5.1 新增文件

```text
src/capture/ImageInputService.h
src/capture/ImageInputService.cpp
```

### 5.2 目标

提供：

```text
选择本地图片 → QImage → OCR → 文本翻译
```

### 5.3 推荐接口

```cpp
class ImageInputService : public QObject
{
    Q_OBJECT

public:
    explicit ImageInputService(QObject* parent = nullptr);

    Q_INVOKABLE void loadImageFile(const QString& filePath);

signals:
    void imageLoaded(QImage image);
    void imageLoadFailed(QString reason);
};
```

### 5.4 要求

- 校验文件是否存在。
- 校验 `QImage` 是否加载成功。
- 错误时发出清晰原因。
- QML 可以使用 `FileDialog` 获取路径，C++ 负责加载校验。

---

## 6. OcrService 抽象

### 6.1 新增文件

```text
src/ocr/OcrService.h
src/ocr/OcrResult.h
```

### 6.2 推荐接口

```cpp
struct OcrResult {
    QString text;
    double confidence = 0.0;
};

class OcrService : public QObject
{
    Q_OBJECT

public:
    explicit OcrService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~OcrService() = default;

    virtual void recognize(const QImage& image) = 0;

signals:
    void recognized(QString text);
    void failed(QString reason);
};
```

---

## 7. ExternalProcessOcrService

### 7.1 新增文件

```text
src/ocr/ExternalProcessOcrService.h
src/ocr/ExternalProcessOcrService.cpp
```

### 7.2 职责

通过外部命令调用 OCR，例如：

```text
tesseract image.png stdout -l eng+chi_sim
```

### 7.3 推荐接口

```cpp
class ExternalProcessOcrService : public OcrService
{
    Q_OBJECT

public:
    explicit ExternalProcessOcrService(QObject* parent = nullptr);

    void setCommand(const QString& command);
    void setLanguage(const QString& language);
    void recognize(const QImage& image) override;
};
```

### 7.4 实现要求

- 使用 `QTemporaryFile` 保存临时图片。
- 使用 `QProcess` 调用外部 `tesseract`。
- 支持配置 command，默认：`tesseract`。
- 支持配置 language，默认：`eng+chi_sim`。
- 命令不存在时发出清晰错误。
- 进程非 0 退出时发出 stderr。
- OCR 输出为空时发出清晰提示或返回空文本，由 coordinator 决定是否翻译。
- 不要阻塞 UI 线程太久；如直接用 `QProcess`，应使用异步信号。

---

## 8. TranslationCoordinator 接入

### 8.1 新增或实现方法

```cpp
Q_INVOKABLE void translateScreenshot();
Q_INVOKABLE void translateImageFile(QString filePath);
```

### 8.2 截图流程

```text
translateScreenshot()
    ↓
ScreenshotCaptureService.captureInteractive()
    ↓
imageCaptured(image)
    ↓
OcrService.recognize(image)
    ↓
recognized(text)
    ↓
TranslationService.translateFromMap(...)
```

### 8.3 图片文件流程

```text
translateImageFile(filePath)
    ↓
ImageInputService.loadImageFile(filePath)
    ↓
imageLoaded(image)
    ↓
OcrService.recognize(image)
    ↓
recognized(text)
    ↓
TranslationService.translateFromMap(...)
```

### 8.4 TranslationRequest

OCR 成功后构造：

```cpp
TranslationRequest {
    source = ScreenshotOcr
    mode = Translate
    text = recognizedText
    targetLanguage = 当前设置或 UI 当前目标语言
    provider = setting.provider
    apiServer = setting.apiServer
    apiKey = setting.apiKey
    model = setting.model
}
```

如果后续区分本地图片来源，可继续使用 `ScreenshotOcr`，或扩展 `TranslationSource`。

---

## 9. QML 接入

### 9.1 第一版展示方式

复用主窗口，不强制 Popup：

```text
OCR 文本填入输入框
显示主窗口
显示翻译流式结果
```

### 9.2 触发方式

至少提供一种可测试入口：

```text
1. 应用内按钮：选择图片翻译
2. 应用内按钮：截图翻译
3. 后续再接快捷键 translateScreenshot
```

如果 `translateScreenshot` 快捷键已存在，可连接到：

```text
TranslationCoordinator.translateScreenshot()
```

### 9.3 FileDialog

QML 可以用 `FileDialog` 选择图片，然后调用：

```qml
coordinator.translateImageFile(selectedFile)
```

路径格式要注意 `file://` URI 与本地路径转换。

---

## 10. 设置项

建议新增配置，但本阶段可以先使用默认值：

```json
"ocr": {
  "backend": "externalTesseract",
  "language": "eng+chi_sim",
  "command": "tesseract"
},
"wayland": {
  "useScreenshotPortal": true
}
```

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

不要因为新增 OCR 配置破坏旧配置加载。

---

## 11. CMake 调整

新增文件后更新：

```text
CMakeLists.txt
```

如实现 Portal D-Bus，需要：

```cmake
find_package(Qt6 REQUIRED COMPONENTS Quick DBus)
```

并链接：

```cmake
target_link_libraries(TransAI PRIVATE Qt6::Quick Qt6::DBus ...)
```

如果只实现本地图片 + ExternalProcessOcrService，不需要 Qt DBus。

---

## 12. 外部依赖说明

本阶段使用外部 OCR 命令时，需要说明依赖：

```text
tesseract
语言包，例如：
- tesseract-ocr-eng
- tesseract-ocr-chi-sim
```

程序运行时应检测命令失败，并提示用户安装，而不是崩溃。

---

## 13. 验收清单

### 13.1 功能验收

- [ ] 手动文本翻译不退化。
- [ ] 选择本地图片后能加载为 `QImage`。
- [ ] 图片加载失败时有明确错误。
- [ ] `ExternalProcessOcrService` 能调用外部 `tesseract`。
- [ ] tesseract 不存在时有明确提示。
- [ ] OCR 输出为空时不发起翻译，并提示用户。
- [ ] OCR 成功后能构造 `TranslationRequest(source = ScreenshotOcr)`。
- [ ] OCR 文本能进入 `TranslationService` 翻译。
- [ ] 翻译结果能在主窗口展示。
- [ ] 如果实现截图，用户取消截图不会报错。
- [ ] 如果实现截图，截图失败不会崩溃。

### 13.2 架构验收

- [ ] `TranslationService` 不包含截图逻辑。
- [ ] `PromptBuilder` 不包含 OCR 或图片逻辑。
- [ ] `OcrService` 不调用翻译服务。
- [ ] `ScreenshotCaptureService` 不调用 OCR 或翻译服务。
- [ ] `TranslationCoordinator` 负责编排截图 / 图片 → OCR → 翻译。
- [ ] 未实现多模态图片翻译。

### 13.3 构建验收

至少执行：

```bash
cmake --build build -j$(nproc)
```

---

## 14. 推荐提交拆分

```text
feat(capture): add image input service
feat(ocr): add OCR service abstraction
feat(ocr): add external tesseract OCR backend
feat(app): route image OCR through translation coordinator
feat(qml): add image translation trigger
feat(capture): add screenshot capture service scaffold
feat(capture): add portal screenshot backend
```

如果 Portal 截图暂不做，可以把最后两个 commit 后置。

---

## 15. 注意事项

1. 第一版可以先通过本地图片完成 OCR 翻译闭环。
2. 不要把多模态 vision 混进本阶段。
3. 不要让 OCR 阻塞 UI。
4. OCR 失败、依赖缺失、图片为空都必须有提示。
5. 截图取消不是错误。
6. Wayland Portal 不可用时必须有 fallback 或明确提示。

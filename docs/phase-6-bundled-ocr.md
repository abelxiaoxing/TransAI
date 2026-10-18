# Phase 6：内置 OCR 后端（RapidOCR / PP-OCR on ncnn）

> 状态：已实现。本文档说明"为什么换、怎么实现、体积与性能的实测数字、如何打包与测试"。

## 1. 背景：为什么不再依赖 tesseract CLI

Phase 2B 引入的 OCR 路径（`src/ocr/TesseractOcrEngine.cpp`）通过 `QProcess`
调用系统 `tesseract`：

- **装不上就用不了**：需要用户自行安装 tesseract 以及 `chi_sim` 语言包；
  在 Arch/Fedora 等发行版上开箱即用时 `which tesseract` 为空，OCR 模式直接
  报不可用（开发机实测即如此）。
- **质量与场景错配**：`--psm 6` + tesseract 对截图里的 UI 小字、深色背景、
  中英混排效果一般。
- **额外进程**：每次识别 spawn 一个进程，还要 1.5s + 2s 的探针超时预算。

Phase 6 的目标：**自带模型、进程内推理、体积可控**，同时保持
`OcrEngine` 的 QML 契约完全不变（`available` / `recognize` / `cancel` /
三个信号），因此 `AppView.qml`、`ScreenshotSection.qml` 的会话逻辑一行未改。

## 2. 方案选型（实测体积）

| 方案 | 新增体积 | 中文质量 | 结论 |
|---|---|---|---|
| tesseract CLI（旧） | 0（但需用户安装，`tessdata_fast` chi_sim 2.47MB + eng 4.11MB） | 中下 | 保留为构建期回退 |
| **RapidOCR + ncnn（本实现）** | **模型 12.9MB + 二进制约 +7MB** | 好 | **采用** |
| RapidOCR + onnxruntime | 模型 13.7MB + 运行时 ~29MB（strip 后实测） | 好 | 运行时太重 |
| PaddleOCR-json 子进程 | release 实测 92–124MB | 好 | 过重 |
| Windows `Windows.Media.Ocr` | 0 | 好 | 仅 Windows，且本项目用 MinGW 构建（C++/WinRT 不可用） |
| macOS Vision framework | 0 | 好 | macOS 继续使用（不编译 ncnn） |

模型来源：RapidOcrNcnn 1.2.0 release 归档（Apache-2.0）里的 PP-OCRv3 mobile
det/rec，ncnn 格式：

| 文件 | 大小 |
|---|---|
| `ch_PP-OCRv3_det_infer.param` / `.bin` | 24KB / 2.30MB |
| `ch_PP-OCRv3_rec_infer.param` / `.bin` | 14KB / 10.53MB |
| `ppocr_keys_v1.txt`（6623 个字符类） | 26KB |

**不使用**角度分类模型 `ch_ppocr_mobile_v2.0_cls`（-0.5MB，且省掉每行一次推理）：
截图文字是水平的，本实现的裁剪框本身就是轴对齐的。

## 3. 实现结构

```
src/ocr/OcrEngine.h           接口 + 三个工厂声明 + unavailableReason 属性
src/ocr/OcrEngineFactory.cpp  createOcrEngine()：VISION / NCNN / TESSERACT / 兜底 四选一
src/ocr/RapidOcrEngine.cpp    ncnn 后端：det → 连通域 → 裁剪 → rec → 拼行
src/ocr/OcrPostProcess.*      纯逻辑（可单测）：分数图→框、阅读顺序、CTC 解码、字典解析
src/ocr/OcrModelLocator.*     模型文件定位（纯逻辑，可单测）
src/ocr/TesseractOcrEngine.cpp 构建期回退（TRANSAI_BUNDLED_OCR=OFF 或 ncnn 不可用）
src/ocr/VisionOcrEngine.mm    macOS
src/ocr/OcrEngineNull.cpp     都没有时自报不可用
```

### 数据流（`RapidOcrEngine::runPipeline`，工作线程）

1. `QImage` → `Format_RGB32`（`bytesPerLine == width*4`，行紧密，ncnn
   `from_pixels_resize` 才能正确处理），`PIXEL_BGRA2BGR` 喂入 **BGR** 平面
   （与 PaddleOCR/OpenCV 参考实现一致）。
2. **检测**：长边缩放到 ≤960、宽高向下取整到 32 的倍数；
   mean `[0.485,0.456,0.406]*255`、norm `1/[0.229,0.224,0.225]/255`；
   `extract("output")` 得到已过 sigmoid 的概率图。
3. **后处理**：`boxThresh 0.3` 二值化 → 8 邻域连通域（显式栈，避免递归爆栈）
   → 均值分数 < `boxScoreThresh 0.5` 丢弃 → 按 PaddleOCR 的 unclip 语义
   （每边外扩 `area * 1.6 / perimeter`）→ 映射回原图坐标并裁剪 → 按行排序。
4. **识别**：每个框缩放到高 48（宽 = 48×宽高比，上限 2048）；
   mean/norm 127.5；`extract("output")` 得 `[T, C]` logits → 贪心 CTC
   （跳过 blank、折叠重复、丢弃 `score < 0.3` 的行）。
5. 行文本 `\n` 连接后 `recognized(text)`。

线程模型：`QtConcurrent::run` + `QFutureWatcher`，`recognize()` 立即返回；
`cancel()` 只递增 generation，完成时 generation 不匹配就丢弃结果（与 macOS
Vision 后端一致）。ncnn `Net` 非线程安全，因此整个 pipeline 在 `QMutex` 下串行。
模型惰性加载（首次识别），加载失败会把 `available` 翻成 false 并
`availableChanged`，UI 提示随之后续更新。

### 模型定位（`OcrModelLocator`）

1. `$TRANSAI_OCR_MODEL_DIR`（**权威覆盖**：一旦设置，不再搜索其它位置）
2. `<可执行文件目录>/ocr/models`（Windows zip / AppImage）
3. `<可执行文件目录>/../share/TransAI/ocr/models`（Linux 安装布局，
   由 `cmake --install` 的 `install(DIRECTORY ...)` 落盘）
4. 编译期注入的源码树路径（开发构建与测试用）

## 4. 体积与性能（x86-64，900×300 截图，3 行文字）

`TRANSAI_OCR_NCNN_ISA` 控制 ncnn 的 SIMD 变体，这是体积的主要变量：

| profile | TransAI 可执行文件 | 检测 + 识别耗时 | 运行时要求 |
|---|---|---|---|
| 关闭 OCR（旧体积） | 1.2MB | — | — |
| `avx-only` | ~4.0MB（strip 后 2.7MB） | ~390ms | 需要 AVX（2011+） |
| `portable`（默认） | 6.9MB（FetchContent 构建实测） | ~390ms | 任意 x86-64（SSE2/AVX 运行时派发） |
| `full` | ~18MB | ~260ms | 任意 x86-64 |

（`avx-only` / `full` 的数字来自用独立构建的 ncnn 链接同一份源代码；模型
12.9MB 是额外落盘的资源，不计入可执行文件。）

结论：`full` 只快约 1.5 倍，却多付 ~10MB；而 OCR 在后台线程里跑、后面还跟着
一次网络翻译请求，390ms 完全可接受 → 默认 `portable`。

准确性对照：与官方 RapidOcrNcnn 1.2.0 的可执行文件在测试图上逐行比对，
结果一致（"HelloTransAl截图翻译" / "Second line: OCR test 123" /
"第三行中文识别"）。`I`/`l` 混淆是 PP-OCR 拉丁字典的固有特性，不是管线缺陷，
端到端测试因此按小写/去空格宽松比对。

## 5. 构建

```bash
./scripts/fetch_ocr_models.sh          # 下载并校验模型 → res/ocr/models（不在 git 内）
cmake -B build -D CMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

- ncnn 通过 `FetchContent` 拉取（`TRANSAI_NCNN_GIT_TAG`，默认 `20260526`），
  CPU-only、无 Vulkan/tools/OpenMP/C API；系统已装 ncnn 时走 `find_package`，
  `-DTRANSAI_OCR_FETCH_NCNN=OFF` 可强制不用网络。
- 首次配置会编译 ncnn（8 核约 5–7 分钟，之后增量缓存）。
- `-DTRANSAI_BUNDLED_OCR=OFF` → 回到 tesseract CLI 后端（旧的用户自装路径）。
- `-DTRANSAI_OCR_MODEL_DIR=...` 指定模型目录。

打包：`build_install.sh`（Linux，`cmake --install` 会带上模型）与
`scripts/windows-*-publish.ps1`（把 `res/ocr/models` 拷进 `ocr/models`）。

## 6. 测试

- `tests/tst_ocrpostprocess.cpp`：分数图→框、unclip 扩张、读序、CTC 解码、
  字典解析、模型定位（含"缺文件即无效"）。**不需要 ncnn、不需要模型**。
- `tests/tst_rapidocr.cpp`：渲染一张图片 → 真识别 → 断言文本；模型缺失时
  `QSKIP`（裸仓库 `ctest` 仍绿），另有用例验证"模型缺失时
  `available=false`、`recognize()` 回 `recognitionFailed(unavailableReason)`"。
- `tests/tst_ocrengine.cpp`：tesseract 回退后端的语言选择逻辑（保留）。

## 7. 已知限制

1. **只做轴对齐框**：斜排文字会被框成一个外接矩形，识别质量下降（截图场景可接受）。
2. **不做 180° 翻转纠正**：没有角度分类模型，倒置文字识别不出来。
3. 检测长边上限 960、识别宽度上限 2048：超大截图里的小字可能漏检。
4. 模型是 **PP-OCRv3 mobile**（2022）；接 PP-OCRv4/v5 只需替换
   `OcrModelLocator` 里的文件名并更新 `scripts/fetch_ocr_models.sh` 的校验值。
5. 首次识别有一次性模型加载开销（~100ms 级），之后常驻内存。

## 8. 相关提交面

- `CMakeLists.txt`：`transai_setup_ocr_backend()` 与四个 cache 选项
- `qml/AppView.qml`、`qml/settings/ScreenshotSection.qml`、`qml/I18n.qml`：
  错误/提示文案改为读取 `ocrEngine.unavailableReason`（固定英文句子兼作
  I18n key，动态细节只进日志）

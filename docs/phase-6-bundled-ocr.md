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
det/rec，ncnn 格式（第三方许可归属见根目录 `THIRD-PARTY-NOTICES.md`）：

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
4. **行重组**（`groupRows` / `mergeRowMembers`）：检测框 ≠ 文本行。检测器会在
   列间距、大空隙处把一个视觉行切成多个框（设置面板的 “Language” 与
   “English”、“[Cancel]” 与 “[Save]”），**每个框单独输出一行**会把 9 行对话框
   变成 17 行、把一墙假的换行词块丢给翻译模型。因此：
   - 同一行（纵向重叠 ≥ 较小高度的 1/2）的成员按 x 排序；
   - 相邻成员的水平间隙 ≤ `0.5 × 行高中位数` 时合并成一个裁剪块（那是被切开的
     同一句话），只识别一次；
   - 间隙更大的（真正的分栏）仍各自识别，随后用空格拼在同一行
     （`joinRowTexts`；两侧都是 CJK 时不加空格）。
5. **识别**：每个裁剪块缩放到高 48（宽 = 48×宽高比，上限 2048）；
   mean/norm 127.5；`extract("output")` 得 `[T, C]` logits → 贪心 CTC
   （跳过 blank、折叠重复、丢弃 `score < 0.3` 的行）。
   **字典布局**（`decodeCtc`）：`ppocr_keys_v1.txt` 没有空格条目，模型按
   `use_space_char=True` 把空格放在**最后一类**（`classes == keys + 2`），
   所以第 `keys+1` 类要还原成空格——否则英文全变
   “ThescreenshotOCRpipeline”。
6. 每行再做一次 CJK↔拉丁字母边界补空格（`normalizeSpacing`，防止模型漏掉
   那个空格；数字不参与，“2024年”“第3章”保持原样），最后 `\n` 连接成
   `recognized(text)`。

线程模型：`QtConcurrent::run` + `QFutureWatcher`，`recognize()` 立即返回；
`cancel()` 只递增 generation，完成时 generation 不匹配就丢弃结果（与 macOS
Vision 后端一致）。ncnn `Net` 非线程安全，因此整个 pipeline 在 `QMutex` 下串行。
模型惰性加载（首次识别），加载失败会把 `available` 翻成 false 并
`availableChanged`，UI 提示随之后续更新。

单个推理内部的并行性来自 **OpenMP**：ncnn 的 kernel 是
`#pragma omp parallel for num_threads(opt.num_threads)`，线程数由
`net.opt.num_threads` 决定，`RapidOcrEngine` 把它固定为
`clamp(idealThreadCount, 1, 4)`。**没有 OpenMP 就没有任何并行**：这个版本的
ncnn 没有回退线程池（`NCNN_SIMPLEOMP=0`），关掉 OpenMP 后实测
`num_threads = 1/2/4/8` 耗时完全相同、`taskset -c 0` 与 8 核也相同，即整条
pipeline 退化成单线程（见下面的实测对比）。

### 模型定位（`OcrModelLocator`）

1. `$TRANSAI_OCR_MODEL_DIR`（**权威覆盖**：一旦设置，不再搜索其它位置）
2. `<可执行文件目录>/ocr/models`（Windows zip / AppImage）
3. `<可执行文件目录>/../share/TransAI/ocr/models`（Linux 安装布局，
   由 `cmake --install` 的 `install(DIRECTORY ...)` 落盘）
4. 编译期注入的源码树路径（开发构建与测试用）

## 4. 体积与性能

### 4.1 SIMD profile（x86-64，900×300 截图，3 行文字，OpenMP 4 线程）

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

### 4.2 线程数（OpenMP，这是决定性变量）

i5-1035G4（4C/8T，`portable` profile，`tests` 同款真实 pipeline，中英混排各
12 行）：

| 线程（`net.opt.num_threads`） | 1080p 12 行 | 1 行 | 仅检测（空图） |
|---|---|---|---|
| 1 | 2.7 s | 0.47 s | 0.26 s |
| 2 | 0.78 s | 0.15 s | 0.10 s |
| **4（默认上限）** | **0.55 s** | 0.14 s | 0.08 s |
| 8 | 0.64 s | 0.14 s | 0.09 s |

- **OpenMP 打开之前**（`NCNN_OPENMP=OFF`）上表四个线程数完全是同一个数字
  （2.4–2.8 s）：`parallel_for` 被编成串行，线程数选项形同虚设。这也是
  `CMakeLists.txt` 里 `NCNN_OPENMP` 不再是开关而是自动启用（找不到 OpenMP 时
  发出警告）的原因。
- 4 线程是上限：再多的线程（超线程）只会带来超额订阅，实测 8 线程反而慢。
- 代价是耗时 ≈ 行数 × 40 ms，不再是 × 200 ms：30 行的大截图从 ~6.6 s 降到
  ~1.3 s，因此“截图翻译要快”的目标不再依赖用户机器的档次。

准确性对照：与官方 RapidOcrNcnn 1.2.0 的可执行文件在测试图上逐行比对，
结果一致（"HelloTransAl截图翻译" / "Second line: OCR test 123" /
"第三行中文识别"）。`I`/`l` 混淆是 PP-OCR 拉丁字典的固有特性，不是管线缺陷，
端到端测试因此按小写/去空格宽松比对。

文本还原（与旧行为对比，`/tmp` 上合成图实测）：

| 场景 | 旧输出 | 现输出 |
|---|---|---|
| 设置面板 9 行、17 个检测框 | 17 行，标签与值各占一行 | 9 行，`Language English`、`[Cancel] [Save]` |
| 英文段落 | `ThescreenshotOCRpipelinedetects...` | `The screenshot OCR pipeline detects...` |
| 中英混排 | `再按PaddleOCR的` | `再按 PaddleOCR 的` |

## 5. 构建

```bash
./scripts/fetch_ocr_models.sh          # 下载并校验模型 → res/ocr/models（不在 git 内）
cmake -B build -D CMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

- ncnn 通过 `FetchContent` 拉取（`TRANSAI_NCNN_GIT_TAG`，默认 `20260526`），
  CPU-only、无 Vulkan/tools/C API；**OpenMP 自动启用**（configure 时
  `find_package(OpenMP)`，找不到则告警并退回单线程）。旧构建树从
  `NCNN_OPENMP=OFF` 切到 ON 会重编 ncnn（约 5–7 分钟）。系统已装 ncnn 时走
  `find_package`；`-DTRANSAI_OCR_FETCH_NCNN=OFF` 可强制不用网络。
- 首次配置会编译 ncnn（8 核约 5–7 分钟，之后增量缓存）。
- `-DTRANSAI_BUNDLED_OCR=OFF` → 回到 tesseract CLI 后端（旧的用户自装路径）。
- `-DTRANSAI_OCR_MODEL_DIR=...` 指定模型目录。

打包：`build_install.sh`（Linux，`cmake --install` 会带上模型；OpenMP 运行时
`libgomp.so.1` 由发型版提供）与 `scripts/windows-*-publish.ps1`（把
`res/ocr/models` 拷进 `ocr/models`；exec 导入 `libgomp-1.dll` / `vcomp140.dll`
时一并拷进包，见 `scripts/release-assets.ps1` 的 `Test-BinaryImportsDll`）。

## 6. 测试

- `tests/tst_ocrpostprocess.cpp`：分数图→框、unclip 扩张、读序、行分组/行内合并、
  CTC 解码（含 `use_space_char` 的空格类）、行内拼接（空格/CJK）、CJK↔拉丁补空格、
  字典解析、模型定位（含“缺文件即无效”）。**不需要 ncnn、不需要模型**。
- `tests/tst_rapidocr.cpp`：渲染一张图片 → 真识别 → 断言文本；通过真实的
  `QQmlEngine` 调一次 `recognize()`（守住 `Q_INVOKABLE` 声明，QML 那边不可调用时
  会话会永久停在“识别中”）；模型缺失时 `QSKIP`（裸仓库 `ctest` 仍绿），
  另有用例验证“模型缺失时 `available=false`、`recognize()` 回
  `recognitionFailed(unavailableReason)`”。
- `tests/tst_ocrengine.cpp`：tesseract 回退后端的语言选择逻辑（保留）。

## 7. 已知限制

1. **只做轴对齐框**：斜排文字会被框成一个外接矩形，识别质量下降（截图场景可接受）。
2. **不做 180° 翻转纠正**：没有角度分类模型，倒置文字识别不出来。
3. 检测长边上限 960、识别宽度上限 2048：超大截图里的小字可能漏检。
4. 模型是 **PP-OCRv3 mobile**（2022）；接 PP-OCRv4/v5 只需替换
   `OcrModelLocator` 里的文件名并更新 `scripts/fetch_ocr_models.sh` 的校验值。
5. 首次识别有一次性模型加载开销（~100ms 级），之后常驻内存。
6. 识别是**整张图一次返回**的：没有逐行进度，UI 只能显示 “Recognizing text...”；
   `cancel()` 也不会中断正在跑的推理，只丢弃结果。4 线程下这个窗口已经很小
   （十几行 < 0.6 s），但截图行数极多时依旧看不到中间进度。
7. 排查“是否真的多线程”的最快方式：看日志里的
   `bundled OCR ready: ... threads: N`（N 就是 `net.opt.num_threads`）。
8. 行重组是**几何启发式**：若同一视觉行里两个框的间隙恰好卡在
   `0.5 × 行高` 附近（如紧凑表格），可能被合成一个裁剪块或反过来被拆开；
   两侧都是 CJK 时不做空格分隔（`joinRowTexts`）。竖排文字、多栏报纸式排版
   不在设计目标内。

## 8. 相关提交面

- `CMakeLists.txt`：`transai_setup_ocr_backend()` 与四个 cache 选项
- `qml/AppView.qml`、`qml/settings/ScreenshotSection.qml`、`qml/I18n.qml`：
  错误/提示文案改为读取 `ocrEngine.unavailableReason`（固定英文句子兼作
  I18n key，动态细节只进日志）

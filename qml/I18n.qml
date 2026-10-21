pragma Singleton
import QtQuick

// UI display language (English / 简体中文).
//
// Deliberately outside the Qt Linguist toolchain (lupdate/lrelease are not
// available in every build environment): strings are looked up in the table
// below with the English source text as the key. The active language follows
// Setting.displayLanguage — exposed to QML as the "setting" context property —
// so switching the entry in the settings page re-evaluates every binding that
// calls t() live, without an application restart.
QtObject {
    // Values are normalized by Setting on load; "en" doubles as the fallback.
    readonly property string language: setting.displayLanguage
    readonly property bool isChinese: language === "zh-CN"

    // zh-CN table. Missing entries fall back to the English source text.
    readonly property var _zhTable: ({
        // Generic
        "Settings": "设置",
        "Saved ✓": "已保存 ✓",
        "LOCAL": "本地",
        "CLOUD": "云端",
        "NO MODEL": "未选模型",
        "Show": "显示",
        "Hide": "隐藏",

        // Language card
        "Language": "语言",
        "DISPLAY LANGUAGE": "界面语言",

        // AI provider card
        "AI Provider": "AI 服务",
        "PROVIDER": "服务商",
        "OpenAI / Compatible API": "OpenAI / 兼容 API",
        "Ollama (Local)": "Ollama（本地）",
        "API SERVER": "API 地址",
        "API KEY": "API 密钥",
        "Required — paste the API key for your provider.": "必填——请粘贴你的服务商 API 密钥。",
        "This key doesn't look like a typical API key. Double-check it if requests fail.": "这个密钥看起来不像常规 API 密钥。如果请求失败，请再检查一遍。",

        // Model card
        "Model": "模型",
        "MODEL": "模型",
        "Scanning": "扫描中",
        "Refresh": "刷新",
        "Detect": "检测",
        "Select from the list, or type a name": "从列表中选择，或直接输入名称",
        "Type a model name, or detect from the server": "输入模型名称，或从服务器检测",
        "No matching models": "没有匹配的模型（可直接输入自定义名称）",
        "Toggle model list": "展开或收起模型列表",
        "%1 models detected — pick one from the dropdown.": "已检测到 %1 个模型——请从下拉列表中选择。",

        // Hotkey card
        "Global Hotkey": "全局快捷键",
        "SELECTION": "划词",
        "SCREENSHOT": "截图",
        "Global hotkeys aren't available on Wayland. Bind shortcuts in your desktop settings to run these commands:": "Wayland 下无法注册全局快捷键。请在桌面环境的快捷键设置中绑定以下命令：",
        "Click to record a shortcut": "点击录制快捷键",
        "Unavailable on this platform": "当前平台不可用",
        "Press keys…": "请按下快捷键…",
        "Esc to cancel": "按 Esc 取消",
        "Couldn't register '%1'. It may be reserved by the system or another app.": "无法注册“%1”。该快捷键可能已被系统或其他应用占用。",
        // Fixed reasons GlobalShortcutManager reports when a chord is already
        // taken (one hotkey = one action).
        "This shortcut is already assigned to Translate selection": "该快捷键已用于“翻译选中文本”",
        "This shortcut is already assigned to Translate screenshot": "该快捷键已用于“截图翻译”",
        "This shortcut is already assigned to another action": "该快捷键已被其他功能占用",

        // Screenshot translation card
        "Screenshot Translation": "截图翻译",
        "Drag to select a region · Esc or right-click to cancel": "拖拽框选截图区域 · Esc 或右键取消",
        "Screen capture is not available on this platform": "当前平台无法截取屏幕",
        "VISION MODEL": "视觉模型",
        "Follow current model": "跟随当前模型",
        "This model may not support image input. Pick a vision-capable model above if screenshot translation fails.": "该模型可能不支持图片输入。如果截图翻译失败，请在上方选择一个视觉模型。",
        "Advanced": "高级",
        "MAX IMAGE WIDTH": "图片最大宽度",
        "JPEG QUALITY": "JPEG 质量",

        // Main view
        "Quick Translate": "快速翻译",
        "Selection unavailable": "无法获取选中文本",
        "Translating...": "翻译中…",
        "Error": "错误",
        "Stopped": "已停止",
        "From history": "来自历史记录",
        "Ready": "就绪",
        "Translated": "翻译结果",
        "Translate (Ctrl+Enter)": "翻译 (Ctrl+Enter)",
        "Translate ⌘Enter": "翻译 ⌘Enter",
        "Copy": "复制",
        "Copied ✓": "已复制 ✓",
        "Stop": "停止",
        "Auto": "自动",
        "History": "历史记录",
        "Clear": "清空",
        "No history yet": "暂无历史记录",
        "[Screenshot]": "[截图]",
        "Screenshot image": "截图图像",
        "Copy command": "复制命令",
        "MODE": "模式",
        "Vision model": "视觉模型",
        "Recognizing text...": "正在识别文字…",
        "OCR timed out: no text was recognized": "OCR 超时：未能识别出文字",
        "No text recognized in the screenshot": "截图中没有识别到文字",
        "OCR is not available on this system: the tesseract command was not found": "OCR 不可用：系统中未找到 tesseract 命令",
        "OCR is not available: the bundled OCR models are missing": "OCR 不可用：内置 OCR 模型文件缺失",
        "OCR is not available: the bundled OCR models could not be loaded": "OCR 不可用：内置 OCR 模型加载失败",
        "OCR is not available on this platform": "当前平台不支持 OCR",
        "OCR failed to start": "OCR 启动失败",
        "Failed to encode the image for OCR": "图片编码失败，无法进行 OCR",
        "Click-outside-to-hide is unavailable: TransAI can't read the pointer devices (/dev/input). See docs/hyprland-integration.md for the udev rule.": "“点击窗口外自动隐藏”不可用：TransAI 无法读取指针设备（/dev/input）。udev 规则配置见 docs/hyprland-integration.md。",

        // API key storage + accessibility names
        "Stored in your system keychain": "已保存在系统密钥环中",
        "Close": "关闭",
        "Translation history": "翻译历史",
        "Copy result": "复制结果",
        "Pin window": "窗口置顶",
        "Unpin window": "取消置顶",

        // App-generated error messages (C++ services emit these English
        // strings; they double as table keys. Dynamic server payloads are
        // not listed here and fall through untranslated on purpose).
        "Please enter text to translate": "请输入要翻译的文本",
        "Please select a model": "请选择模型",
        "Please provide the correct apikey": "请提供正确的 API 密钥",
        "Translation timed out: no data from the server": "翻译超时：服务器没有返回数据",
        "Translation timed out: the server took too long to respond": "翻译超时：服务器响应时间过长",
        "Network error: could not reach the server": "网络错误：无法连接服务器",
        "Proxy error: could not reach the server through the proxy": "代理错误：无法通过代理连接服务器",
        "Request rejected: check the API key, model name and server URL": "请求被拒绝：请检查 API 密钥、模型名称和服务器地址",
        "Protocol error: unexpected response from the server": "协议错误：服务器返回了意外的响应",
        "Server error: the API server failed to handle the request": "服务器错误：API 服务器处理请求失败",
        "Network error": "网络错误",
        "No screenshot image was provided": "没有提供截图",
        "The captured image is empty": "截取的图片为空",
        "The captured image is too small": "截取的图片过小",
        "Failed to encode the image as JPEG": "图片 JPEG 编码失败",
        "Clipboard is unavailable": "剪贴板不可用",
        "Failed to simulate copy": "模拟复制失败",
        "Clipboard did not change after copy": "复制后剪贴板内容未变化",
        "No selected text found": "未检测到选中文本",
        "The selected region is outside the captured screen": "所选区域不在截取的屏幕范围内",
        "The selected model may not support image input. Open Settings and choose a vision-capable model for screenshot translation.": "当前模型可能不支持图片输入。请打开设置，为截图翻译选择一个视觉模型。",

        // Tray menu
        "Translate selection": "翻译选中文本",
        "Translate screenshot": "截图翻译",
        "Quit": "退出"
    })

    // Lookup with optional %1, %2, … substitution: t("%1 models detected", 3).
    function t(text) {
        var result = isChinese && _zhTable[text] !== undefined ? _zhTable[text] : text
        for (var i = 1; i < arguments.length; ++i) {
            result = result.split("%" + i).join(arguments[i])
        }
        return result
    }
}

---
project_name: "TransAI"
template_version: "2.0"
creation_date: "2025-09-26"
author: "AI Assistant"
version: "1.0"

# UX Goals & Principles
ux_goals:
  personas:
    - "**专业翻译用户:** 经常需要翻译文档和对话的专业人士，注重翻译质量和效率"
    - **普通用户:** 偶尔需要翻译的日常用户，希望界面简单易用"
    - **开发者:** 需要翻译技术文档的开发者，支持技术术语准确翻译"

  usability_goals:
    - 易用性: 新用户首次使用时，能在2分钟内完成第一次翻译操作
    - 效率: 熟练用户能在5秒内完成选择文本到获得翻译结果的全流程
    - 可访问性: 支持键盘快捷键和系统级集成操作
    - 记忆性: 用户可以轻松记忆和重复使用核心功能

  design_principles:
    1. **即时响应** - 任何用户操作都应立即获得视觉反馈
    2. **专注核心** - 界面元素围绕翻译功能，避免干扰性设计
    3. **渐进展示** - 根据用户操作逐步显示相关信息和选项
    4. **一致性设计** - 保持与系统视觉风格和交互模式的一致性
    5. **性能优先** - 所有动画和交互都不应影响翻译效率

# Information Architecture
sitemap: |
  graph TD
      A["主窗口 (MainWindow)"] --> B["翻译视图 (AppView)"]
      A --> C["设置视图 (SettingView)"]
      A --> D["系统托盘 (SystemTray)"]
      A --> E["弹窗翻译 (PopWindow)"]

      B --> B1["输入区域"]
      B --> B2["翻译结果区域"]
      B --> B3["语言选择器"]
      B --> B4["翻译按钮"]
      B --> B5["窗口置顶控制"]

      C --> C1["API配置"]
      C --> C2["快捷键设置"]
      C --> C3["模型选择"]
      C --> C4["更新检查"]

      D --> D1["托盘菜单"]
      D --> D2["快捷键入口"]

      E --> E1["剪贴板内容"]
      E --> E2["翻译显示"]

      click B "./qml/AppView.qml" "查看翻译视图实现"
      click C "./qml/SettingView.qml" "查看设置视图实现"

navigation_structure:
  primary: "SwipeView 在翻译视图和设置视图之间切换"
  secondary: "通过设置按钮和返回按钮在主视图间导航"
  breadcrumbs: "不使用面包屑，采用线性导航结构"

# User Flows
user_flows:
  - name: "核心翻译流程"
    goal: "用户通过选择文本快速获取翻译结果"
    entry_points: "系统托盘图标点击、全局快捷键触发、主窗口直接输入"
    success_criteria: "从选择文本到显示翻译结果的时间不超过3秒"
    diagram: |
      graph TD
          A[选择文本] --> B[触发快捷键]
          B --> C[自动填充输入框]
          C --> D[自动开始翻译]
          D --> E[显示翻译结果]
          E --> F[可选复制结果]

          G[打开主窗口] --> H[手动输入文本]
          H --> I[点击翻译按钮]
          I --> E

          J[设置界面] --> K[配置API和快捷键]
          K --> L[返回主窗口]
          L --> G

    edge_cases:
      - 网络连接失败时的错误处理和重试机制
      - API密钥无效或过期时的友好提示
      - 长文本翻译的流式显示和进度反馈
      - 多语言混合文本的智能识别和处理

  - name: "设置配置流程"
    goal: "用户配置翻译API、快捷键和界面选项"
    entry_points: "主界面设置按钮点击"
    success_criteria: "用户能在2分钟内完成所有必要配置"
    diagram: |
      graph TD
          A[点击设置按钮] --> B[切换到设置视图]
          B --> C[配置API服务器]
          C --> D[输入API密钥]
          D --> E[选择翻译模型]
          E --> F[设置快捷键]
          F --> G[保存配置]
          G --> H[返回翻译视图]
          H --> I[测试配置效果]

    edge_cases:
      - API配置验证失败时的实时反馈
      - 快捷键冲突检测和提示
      - 配置丢失或损坏时的恢复机制
      - 版本更新时的配置迁移

# Component Library
design_system_approach: "基于Qt Quick的自定义组件库，采用Material Design语言适配深色主题，支持动画和状态变化"

core_components:
  - name: "StyledButton (样式按钮)"
    purpose: "提供统一的按钮样式和交互反馈"
    variants: "主要操作按钮、次要按钮、图标按钮"
    states: "默认、悬停、按下、禁用、加载中"
    usage_guidelines: "用于主要操作如翻译、设置导航等"

  - name: "GTextEdit (文本编辑器)"
    purpose: "支持多行文本输入和显示的编辑器组件"
    variants: "输入模式、只读模式、自动滚动模式"
    states: "聚焦、非聚焦、内容变化、滚动状态"
    usage_guidelines: "用于文本输入和翻译结果显示"

  - name: "IconButton (图标按钮)"
    purpose: "提供图标化的操作按钮"
    variants: "设置按钮、返回按钮、置顶按钮"
    states: "正常、悬停、按下、禁用"
    usage_guidelines: "用于工具栏和快捷操作"

  - name: "ThemeColors (主题颜色系统)"
    purpose: "提供统一的深色主题颜色和设计令牌"
    variants: "主要颜色、状态颜色、设计参数"
    states: "静态配置，运行时不可变"
    usage_guidelines: "所有UI组件的颜色配置都从此获取"

# Branding & Style
visual_identity: "专业的深色主题设计语言，注重可读性和视觉舒适度"

color_palette:
  - type: "Primary"
    hex: "#4EC9B0"
    usage: "主要强调色，用于按钮、链接和重要元素"
  - type: "Background"
    hex: "#1E1E1E"
    usage: "主背景色，提供良好的对比度"
  - type: "Background Secondary"
    hex: "#252526"
    usage: "次要背景色，用于输入框和卡片"
  - type: "Foreground"
    hex: "#D4D4D4"
    usage: "主要文本颜色，确保可读性"
  - type: "Border"
    hex: "#3E3E42"
    usage: "边框和分割线颜色"
  - type: "Success"
    hex: "#4EC9B0"
    usage: "成功状态和积极反馈"
  - type: "Warning"
    hex: "#D4A76A"
    usage: "警告和注意事项"
  - type: "Error"
    hex: "#E84855"
    usage: "错误状态和警示信息"

typography:
  font_families:
    primary: "Segoe UI, Arial, sans-serif"
    secondary: "系统默认字体"
    monospace: "Consolas, Monaco, monospace"

  type_scale:
    - element: "H1"
      size: "24px"
      weight: "Bold"
      line_height: "1.2"
    - element: "H2"
      size: "18px"
      weight: "Medium"
      line_height: "1.3"
    - element: "Body"
      size: "14px"
      weight: "Regular"
      line_height: "1.5"
    - element: "Small"
      size: "12px"
      weight: "Regular"
      line_height: "1.4"

iconography:
  icon_library: "自定义图标库 + Qt内置图标"
  usage_guidelines: "使用线性图标，确保在深色背景下的清晰度，保持18x18px的标准尺寸"

spacing_layout:
  grid_system: "8px基础网格系统，支持4/8/12/16/24px的标准间距"
  spacing_scale: "4px (xs), 8px (sm), 12px (md), 16px (lg), 24px (xl)"

# Dark Theme Specification
dark_theme:
  philosophy: "采用高对比度深色主题，减少视觉疲劳，提高长时间使用的舒适度"

  color_strategy: "基于VS Code深色主题的色彩体系，确保文本可读性和层次感"

  accessibility: "所有文本元素对比度不低于4.5:1，重要操作元素对比度不低于7:1"

  animation_principles: "动画时长控制在150-350ms，采用缓动函数确保自然的视觉体验"

# Accessibility Requirements
compliance_target: "WCAG 2.1 AA级别标准，支持键盘导航和屏幕阅读器"

key_requirements:
  visual:
    contrast_requirements: "正常文本对比度≥4.5:1，大文本≥3:1"
    focus_requirements: "所有可交互元素都有明显的焦点指示器"
    text_requirements: "支持系统字体大小设置，最小12px"

  interaction:
    keyboard_requirements: "完整键盘导航支持，Tab键顺序合理"
    screen_reader_requirements: "所有重要元素都有适当的ARIA标签"
    touch_requirements: "触摸目标尺寸不小于44x44px"

  content:
    alt_text_requirements: "所有图标都有描述性替代文本"
    heading_requirements: "使用语义化的标题结构"
    form_requirements: "所有表单元素都有明确的标签"

testing_strategy: "使用Qt内置的辅助功能测试工具，配合手动键盘导航测试"

# Responsiveness Strategy
breakpoints:
  - breakpoint: "Mobile"
    min_width: "320px"
    max_width: "480px"
    target_devices: "手机和小型平板"
  - breakpoint: "Tablet"
    min_width: "481px"
    max_width: "768px"
    target_devices: "平板设备"
  - breakpoint: "Desktop"
    min_width: "769px"
    max_width: "1920px"
    target_devices: "桌面电脑"
  - breakpoint: "Wide"
    min_width: "1921px"
    max_width: "-"
    target_devices: "大屏幕显示器"

adaptation_patterns:
  layout_adaptations: "窗口最小尺寸400x500px，支持窗口调整大小时自动重排"
  nav_adaptations: "在小屏幕上简化导航，优先显示核心功能"
  content_adaptations: "长文本内容自动换行，保持良好的阅读体验"
  interaction_adaptations: "触摸设备优化按钮尺寸和间距"

# Animation & Micro-interactions
motion_principles: "动画应该有明确的目的，要么提供状态反馈，要么引导用户注意力"

key_animations:
  - name: "按钮交互动画"
    description: "按钮悬停和按下时的颜色渐变效果"
    duration: "150ms"
    easing: "OutQuad"
  - name: "输入框聚焦动画"
    description: "输入框获得焦点时边框颜色变化"
    duration: "200ms"
    easing: "OutQuad"
  - name: "窗口显示动画"
    description: "主窗口和弹窗的淡入效果"
    duration: "250ms"
    easing: "OutCubic"
  - name: "文本加载动画"
    description: "翻译结果流式显示的打字机效果"
    duration: "实时"
    easing: "线性"

# Performance Considerations
performance_goals:
  page_load: "应用启动时间<1秒，窗口显示<300ms"
  interaction_goal: "按钮点击响应<100ms，文本输入延迟<50ms"
  animation_goal: "所有动画维持60FPS，避免掉帧"

design_strategies: |
  使用QML的内置性能优化机制：
  - 避免频繁的属性绑定和复杂的JavaScript计算
  - 使用适当的缓存策略减少重复计算
  - 优化图片资源，使用合适的尺寸和格式
  - 合理使用动画，避免过度效果影响性能

# Next Steps
immediate_actions:
  1. 完善所有组件的样式文档和使用示例
  2. 创建交互原型，验证用户流程的可用性
  3. 制定无障碍性测试计划和检查清单
  4. 准备设计规范，确保开发实现的一致性

design_handoff_checklist:
  - "所有用户流程已完整文档化"
  - "组件库规范已建立"
  - "深色主题颜色系统已定义"
  - "无障碍性要求已明确"
  - "性能目标已设定"
  - "动画规范已制定"

# Document Version History
changelog:
  - date: "2025-09-26"
    version: "1.0"
    description: "初始版本，包含完整的UI/UX规格说明"
    author: "AI Assistant"
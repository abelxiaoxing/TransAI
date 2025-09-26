# 整体架构

## 架构概述

```
TransAI 应用架构
├── 前端层 (Frontend Layer)
│   ├── QML 界面组件
│   ├── 深色主题系统
│   ├── 交互控制器
│   └── 动画效果管理
├── 业务逻辑层 (Business Logic Layer)
│   ├── 翻译控制器 (Controller)
│   ├── 热键管理器 (Hotkey)
│   ├── 划词翻译服务
│   └── 主题管理器
├── 数据层 (Data Layer)
│   ├── 配置管理 (Settings)
│   ├── 主题配置
│   └── 用户偏好
└── 系统集成层 (Integration Layer)
    ├── 系统剪贴板
    ├── 全局热键
    ├── 文本选择检测
    └── OpenAI API
```

## 技术栈

- **前端**: Qt 6 + QML + Qt Quick Controls
- **后端**: C++17 + Qt Framework
- **构建**: CMake 3.16+
- **第三方库**: QHotkey (热键功能)
- **API**: OpenAI GPT API
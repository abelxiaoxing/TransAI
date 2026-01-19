# AGENTS.md - TransAI Project Guide

## Project Overview
TransAI is a cross-platform translation application built with Qt6 and QML that provides AI-powered translation services. The application features a modern dark theme UI, global hotkey support, system tray integration, and automatic window management.

## Build Commands

### Prerequisites
- Qt 6.9.2 or compatible version
- CMake 3.16+
- C++17 compatible compiler

### Build Process
```bash
# Configure and build
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_PREFIX_PATH="$QT_PATH" ..
make -j$(nproc)

# Install (Linux)
sudo make install
sudo ldconfig

# Build using automation script
./build_install.sh
```

### Platform-Specific Setup
- **Linux**: Requires X11 development libraries
- **Windows**: Uses MinGW or MSVC toolchain
- **macOS**: Requires macOS SDK and ApplicationServices framework

## Project Structure

### Source Organization
```
src/
├── main.cpp              # Application entry point
├── controller.h/cpp      # Main business logic and API communication
├── hotkey.h/cpp          # Global hotkey management
├── updater.h/cpp         # Application update functionality
├── stdafx.h              # Common macros and precompiled headers
└── theme/
    ├── ThemeManager.h/cpp # Dark/light theme management system

qml/
├── main.qml              # Main application window
├── AppView.qml           # Primary translation interface
├── SettingView.qml       # Settings/preferences UI
├── GPopWindow.qml        # Popup window component
└── StyledComponent.qml   # Reusable UI components

res/                      # Resource files (images, icons)
lib/                      # Third-party dependencies (QHotkey)
docs/                     # Architecture and requirements documentation
```

## Key Patterns and Conventions

### C++ Coding Standards
- **Header Guards**: Use `#ifndef CLASSNAME_H` / `#define CLASSNAME_H` pattern
- **QML Integration**: Use `qmlRegisterType<ClassName>("Module",1,0,"TypeName")` for QML exposure
- **Properties**: Use `Q_PROPERTY_AUTO(TYPE, member)` macro from `stdafx.h` for automatic property generation
- **Signals/Slots**: Follow Qt naming conventions (`memberChanged()` signals)

### QML Patterns
- **Theme Colors**: Centralized color definitions in `AppView.qml` using property aliases
- **Window Management**: Use `ensureWindowVisible()` function to keep windows on screen
- **Component Lifecycle**: Properly manage dynamic component creation/destruction for popup windows
- **Platform Detection**: Use `Qt.platform.os` for platform-specific UI adjustments

### Theme System Architecture
- **ThemeManager**: Central C++ class managing dark/light theme state
- **ThemeController**: QML-accessible wrapper for theme operations
- **Color Palette**: Defined in QML with fallback values for theme colors
- **Persistence**: Theme state saved via QSettings and restored on startup

## Configuration Management

### Settings Storage
- **Location**: Platform-specific config directories (AppConfigLocation)
- **Format**: JSON configuration file (`config.json`)
- **Properties**: API key, server URL, model selection, shortcut keys

### Key Settings
```json
{
  "apiKey": "your-openai-api-key",
  "apiServer": "https://api.openai.com",
  "model": "gpt-3.5-turbo",
  "shortCut": "Ctrl+Shift+T"
}
```

## Important Gotchas

### Platform-Specific Issues
1. **Linux/Wayland**: Application defaults to software rendering backend due to graphics pipeline issues
2. **Linux/Hyprland**: QWaylandDataOffer timeout errors fixed in recent commits
3. **macOS**: Requires ApplicationServices framework for accessibility features
4. **Windows**: Uses resource files for application icons and metadata

### Window Management
- **Always On Top**: Application defaults to `WindowStaysOnTopHint` flag on startup
- **Auto-Close**: Main window hides with fade animation when losing focus (unless pinned)
- **Screen Boundaries**: Window position automatically constrained to visible screen area

### Hotkey System
- **Global Shortcuts**: Uses QHotkey library for system-wide hotkey registration
- **Configuration**: Shortcut can be customized via settings
- **Activation**: Hotkey triggers text translation from anywhere in the system

### API Communication
- **Streaming**: Uses QNetworkAccessManager for streaming API responses
- **Error Handling**: Comprehensive error parsing for API failures
- **Abort Support**: Requests can be cancelled mid-translation

## Testing Strategy

### Current State
- No formal test suite exists in the repository
- Manual testing guided by QA gates in `docs/qa/gates/`
- Architecture designed for testability with clear separation of concerns

### Recommended Test Areas
1. **Theme System**: Dark/light theme switching and persistence
2. **Hotkey Registration**: Cross-platform shortcut handling
3. **API Integration**: Network request/response handling
4. **Configuration**: Settings load/save operations
5. **Window Behavior**: Auto-hide, pinning, and screen boundary constraints

## Build and Deployment Scripts

### Linux Build Script (`build_install.sh`)
- Automated Qt path detection
- Release build configuration
- System installation with proper RPATH settings
- Dynamic library cache refresh

### Windows Build Scripts
- PowerShell scripts for MinGW and MSVC environments
- Static linking configuration options
- Resource compilation for application metadata

## Documentation References

### Architecture Documents
- `docs/architecture.md` - Overall technical architecture
- `docs/architecture/` - Detailed implementation guides
- `docs/prd.md` - Product requirements and feature specifications
- `docs/stories/` - User stories and implementation details

### Key Features Documentation
- Dark theme system implementation
- Input box interaction optimization
- Automatic window closing behavior
- Word selection translation functionality

## Development Workflow

### Git Conventions
- Feature branches for new functionality
- Descriptive commit messages in English or Chinese
- Regular integration with main branch

### Code Organization
- C++ backend logic with QML frontend
- Clear separation between business logic and UI
- Resource files embedded via Qt resource system
- Internationalization support for multiple languages

## Common Development Tasks

### Adding New QML Components
1. Create component in `qml/` directory
2. Import necessary QtQuick modules
3. Follow existing theme color patterns
4. Test across all target platforms

### Modifying Theme System
1. Update `ThemeManager` for new color properties
2. Modify QML color definitions in `AppView.qml`
3. Test theme switching functionality
4. Verify persistence across application restarts

### Extending Settings
1. Add properties to `Setting` class
2. Update JSON serialization in `loadConfig()`/`updateConfig()`
3. Add UI controls in `SettingView.qml`
4. Test configuration persistence

This guide provides essential information for agents working on the TransAI project. Refer to specific documentation files for detailed implementation guidelines.
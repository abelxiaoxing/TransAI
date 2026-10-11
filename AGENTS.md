# AGENTS.md - TransAI Project Guide

## Project Overview
TransAI is a cross-platform AI-powered translation application built with Qt6 and QML. It features global hotkey support, system tray integration, single-instance command forwarding, and selection translation on X11 (simulated copy) and Wayland (primary selection).

## Build Commands

### Prerequisites
- Qt 6.9+ (tested with 6.11)
- CMake 3.16+
- C++17 compatible compiler
- X11 + XTest development libraries (Linux)

### Build Process
```bash
# Configure and build
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_PREFIX_PATH="$QT_PATH" ..
make -j$(nproc)

# Install (Linux, user-level, no root)
./build_install.sh
```

### Platform-Specific Setup
- **Linux**: Requires X11 and XTest (`pkg-config xtst`); software rendering backend by default (override with `QT_QUICK_BACKEND` env var before launch)
- **Windows**: Uses MinGW or MSVC toolchain (`scripts/windows-*.ps1`)
- **macOS**: Requires macOS SDK and ApplicationServices framework

## Project Structure

### Source Organization
```
src/
├── main.cpp                        # Entry point, CLI, single-instance bootstrap
├── stdafx.h                        # Q_PROPERTY_AUTO macro and common defs
├── app/
│   ├── ApplicationCommandServer.*  # Local socket server for single-instance commands
├── input/
│   ├── GlobalShortcutManager.*     # QHotkey-based global hotkeys (QML type; disabled on Wayland)
│   ├── SelectionTextProvider.*     # Selected-text extraction per platform (QML type)
│   └── ClipboardGuard.*            # Save/restore clipboard around simulated copy
├── providers/
│   ├── ProviderDefaults.h          # Shared provider defaults (server URLs, api-key check)
│   ├── ProviderEndpointBuilder.*   # Chat/models URL normalization per provider
│   ├── ModelDiscoveryService.*     # /v1/models + ollama /api/tags detection (QML type)
│   └── ThinkingPolicy.*            # Per-vendor "suppress thinking" request params
├── settings/
│   └── Setting.*                   # config.json load/save + active provider config
├── translation/
│   ├── TranslationService.*        # Streaming chat-completion client (QML type)
│   ├── StreamParser.*              # SSE chunk parser (OpenAI-compatible + raw JSON)
│   ├── TranslationRequest.*        # Request struct + QVariantMap conversion
│   └── PromptBuilder.*             # Per-mode system/user prompt construction
qml/
├── main.qml                        # Main window, tray, drag, shortcuts, command dispatch
├── AppView.qml                     # Translation interface
├── SettingView.qml                 # Settings UI (provider/model/hotkey)
├── AppBackground.qml               # Shared gradient + neon-circle page background
├── ThemedComboBox.qml              # Shared dark-theme ComboBox (settings page)
├── GTextEdit.qml                   # Flickable-based text editor
├── IconButton.qml                  # Image-based stateful button
├── ThemeColors.qml                 # Dark-palette singleton (registered in qmldir)
└── qmldir                          # QML module registration
lib/                                # Third-party dependencies (QHotkey)
docs/                               # Phased implementation blueprints
scripts/                            # Windows build/publish scripts
```

## Key Patterns and Conventions

### C++ Coding Standards
- **Header Guards**: `#ifndef CLASSNAME_H` / `#define CLASSNAME_H`
- **QML Integration**: `qmlRegisterType<T>("TransAI", 1, 0, "TypeName")` in main.cpp
- **Properties**: `Q_PROPERTY_AUTO(TYPE, member)` macro from `stdafx.h` (const getter, const-ref setter)
- **Services**: QML-instantiated types expose signals (`translationStarted/Finished/Failed/Stopped`)

### QML Patterns
- **Theme Colors**: Single source of truth is the `ThemeColors` singleton (qml/ThemeColors.qml). Views alias needed tokens as readonly properties; do not hardcode palette hex values.
- **Shared chrome**: Page backgrounds come from `AppBackground`; settings-page dropdowns use `ThemedComboBox`. Extend those components instead of copying their styles.
- **Declarative state**: Avoid imperative assignments that break bindings (e.g. opacity driven by MouseArea state must stay a binding).
- **Window Management**: `ensureWindowVisible()` keeps windows on screen; auto-hide on focus loss unless pinned
- **Platform Detection**: `Qt.platform.os` for platform-specific UI adjustments

### Settings Storage
- Location: `QStandardPaths::AppConfigLocation` (`~/.config/TransAI/config.json` on Linux)
- Format: JSON with `configVersion`; per-provider keys (`openaiApiServer/openaiApiKey/openaiModel`, `ollamaApiServer/ollamaModel`) plus active `provider`, `thinkingMode`, `shortCut`. Legacy top-level `apiKey/apiServer/model` are READ as one-way migration fallbacks but no longer written
- API keys are stored in plain text; the file is chmod 600 on Unix

### Single-Instance & Selection Translation
- `TransAI --translate-selection` / `--background` / plain launch: command forwarded over a local socket (`$XDG_RUNTIME_DIR/transai-command`) to the running instance; first instance becomes the server
- Wayland: QHotkey is disabled; users bind their own desktop hotkey to `TransAI --translate-selection`
- X11: selection read via simulated Ctrl+C with clipboard guard; Wayland: `wl-paste --primary` with Qt primary-selection fallback

### API Communication
- **Streaming**: SSE parsing with cross-chunk buffering; `[DONE]` handling; raw-JSON fallback for error bodies
- **Timeouts**: 30s inactivity (translation), 15s (model detection)
- **Error model**: one terminal signal per request (`translationFinished` / `translationFailed` / `translationStopped`)
- **Thinking suppression**: `ThinkingPolicy` picks vendor-specific params (DeepSeek `thinking.type`, Qwen `enable_thinking`, Ollama `think`, OpenAI `reasoning_effort`) plus `/no_think` and direct-answer prompt hints

## Important Gotchas

### Platform-Specific Issues
1. **Linux**: `QT_QUICK_BACKEND=software` is the default (GPU pipeline failures); users can override the env var
2. **Wayland**: global hotkey registration is not available; use the command-server path
3. **macOS**: ApplicationServices framework required for simulated copy
4. **Qt logging on systemd distros**: qDebug output goes to the systemd journal, not stderr; use `journalctl --user` when debugging, or `QT_FORCE_STDERR_LOGGING=1`
5. **Headless platforms (offscreen/minimal/dummy)**: GlobalShortcutManager refuses registration there (QHotkey's X11 backend segfaults); smoke tests log a shortcutRegisterFailed instead of crashing

### Window Management
- Always-on-top is enabled by default at startup; the pin toggle mutates `mainWindow.flags`
- Main window hides with fade animation on focus loss (unless pinned)

## Testing Strategy

### Current State
- No formal test suite; manual testing guided by `docs/qa/gates/` when applicable
- Smoke test without a display:
  ```bash
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/TransAI --background
  QT_QPA_PLATFORM=offscreen ./build/TransAI --translate-selection   # from a 2nd process
  ```
- Check QML/engine errors via `journalctl --user` (see logging gotcha above)

### Recommended Test Areas
1. Provider switch openai ↔ ollama and config persistence
2. Model detection against OpenAI-compatible and Ollama endpoints
3. Hotkey re-registration from the settings page
4. Streaming translation abort (partial result + Stopped status)
5. Single-instance command forwarding with a stale socket file

## Documentation References

- `docs/phase-1-architecture-refactor.md` - Module split blueprint (implemented)
- `docs/phase-2*.md` - Selection/OCR/UI blueprints (2A implemented; 2B/2C/2D pending)
- `docs/hyprland-integration.md` - Wayland compositor notes

## Development Workflow

### Git Conventions
- Feature branches; descriptive commit messages in English or Chinese
- Conventional-commit style seen in history: `feat(scope):`, `refactor(scope):`, `docs:`, `style(qml):`

### Code Organization
- C++ backend logic with QML frontend; clear separation between services and UI
- Resources embedded via Qt resource system (TransAI.qrc)
- i18n: translator loading is wired in main.cpp but no .qm files are bundled yet; UI strings are largely untranslated (see TransAI_zh_CN.ts)

## Common Development Tasks

### Adding New QML Components
1. Create the component in `qml/`
2. Import required modules; reference `ThemeColors` for palette tokens
3. Add the file to `TransAI.qrc` (and `qml/qmldir` if it is a singleton)
4. Test on all target platforms

### Extending Settings
1. Add keys to `Setting::loadConfig()`/`updateConfig()` (keep legacy keys for migration)
2. Add UI controls in `SettingView.qml`; use `saveConfigSoon()` for text inputs (debounced)
3. Verify persistence across restarts

This guide reflects the codebase as of the Phase 1 refactor + Wayland selection work. Update it when modules move.

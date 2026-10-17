# AGENTS.md - TransAI Project Guide

## Project Overview
TransAI is a cross-platform AI-powered translation application built with Qt6 and QML. It features global hotkey support, system tray integration, single-instance command forwarding, and selection translation on X11 (simulated copy) and Wayland (primary selection).

## Build Commands

### Prerequisites
- Qt 6.9+ (tested with 6.11), with Quick + Network + DBus (DBus is Linux-only, for portal screenshots)
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
├── PropertyMacros.h                 # Q_PROPERTY_AUTO macro and common defs
├── app/
│   ├── ApplicationCommandServer.*  # Local socket server for single-instance commands
│   ├── ClipboardHelper.*           # QML-facing QClipboard wrapper (context property "clipboard"; copies result text without a selection)
│   └── TranslationHistory.*        # Recent-translation store (context property "translationHistory"; translation-history.json, newest-first, capped at 100)
├── input/
│   ├── GlobalShortcutManager.*     # QHotkey-based global hotkeys (QML type; disabled on Wayland)
│   ├── GlobalClickMonitor.*        # Passive evdev BTN_LEFT observer for click-outside-to-hide on Wayland (context property; needs /dev/input read access)
│   ├── SelectionTextProvider.*     # Selected-text extraction per platform (QML type)
│   └── ClipboardGuard.*            # Save/restore clipboard around simulated copy
├── providers/
│   ├── ProviderDefaults.h          # Shared provider defaults (server URLs, api-key check)
│   ├── ApiError.h                  # Shared OpenAI-style error-payload message extraction
│   ├── ProviderEndpointBuilder.*   # Chat/models URL normalization per provider (static helpers)
│   ├── ModelDiscoveryService.*     # /v1/models + ollama /api/tags detection (QML type)
│   └── ThinkingPolicy.*            # Automatic thinking-suppression policy (params + rejection retry)
├── settings/
│   ├── Setting.*                   # config.json load/save + active provider config
│   └── SecretStore*.h/cpp/mm       # OS keychain for the API key (libsecret / Keychain / DPAPI; null fallback)
├── ocr/
│   ├── OcrEngine.h                 # OCR backend interface + language-selection helpers (context property "ocrEngine")
│   ├── TesseractOcrEngine.cpp      # Linux/Windows backend: tesseract CLI via QProcess (optional at runtime)
│   ├── VisionOcrEngine.mm          # macOS backend: VNRecognizeTextRequest (Vision framework)
│   └── OcrEngineNull.cpp           # Unavailable fallback backend
├── translation/
│   ├── StreamingChatClient.*       # Shared streaming chat-completion base (SSE, timeouts, thinking retry, terminal signals)
│   ├── TranslationService.*        # Text translation (StreamingChatClient subclass: validation + PromptBuilder body)
│   ├── StreamParser.*              # SSE chunk parser (OpenAI-compatible + raw JSON)
│   ├── TranslationRequest.*        # Request struct + TranslationMode int mapping
│   └── PromptBuilder.*             # Per-mode system/user prompt construction
├── capture/
│   ├── ScreenshotCaptureService.*  # Unified screenshot entry (QML type): Wayland portal vs QScreen fallback
│   └── PortalScreenshotBackend.*   # xdg-desktop-portal Screenshot over D-Bus (Linux/Wayland)
├── vision/
│   ├── VisionTranslationService.*  # Multimodal translation (StreamingChatClient subclass: image payload + vision messages + error guidance)
│   ├── ImagePayloadBuilder.*       # QImage → scaled JPEG → base64 data URL (pure logic)
│   └── VisionModelCapability.h     # Header-only vision-model id heuristic (UI hints/routing only)
qml/
├── main.qml                        # Main window, tray, drag, shortcuts, command dispatch
├── AppView.qml                     # Translation interface
├── SettingView.qml                 # Settings page skeleton: header + section wiring, load/save orchestration
├── settings/                       # Setting sections: Language/Provider/Model/Screenshot/HotkeySection +
│                                   #   shared SectionCard/SettingsRow/StatusPill/RoundIconButton/ShortcutRecorder
├── AppBackground.qml               # Shared gradient + neon-circle page background
├── ThemedComboBox.qml              # Shared dark-theme ComboBox (settings page)
├── GTextEdit.qml                   # Flickable-based text editor
├── IconButton.qml                  # Image-based stateful button
├── ThemeColors.qml                 # Dark-palette singleton (registered in qmldir)
├── I18n.qml                        # UI-language singleton (en / zh-CN, registered in qmldir)
├── qmldir                          # QML module registration
lib/                                # Third-party dependencies (QHotkey)
docs/                               # Phased implementation blueprints
scripts/                            # Windows build/publish scripts
```

## Key Patterns and Conventions

### C++ Coding Standards
- **Header Guards**: `#ifndef CLASSNAME_H` / `#define CLASSNAME_H`
- **QML Integration**: `qmlRegisterType<T>("TransAI", 1, 0, "TypeName")` in main.cpp
- **Properties**: `Q_PROPERTY_AUTO(TYPE, member)` macro from `src/PropertyMacros.h` (const getter, const-ref setter)
- **Services**: QML-instantiated types expose signals (`translationStarted/Finished/Failed/Stopped`)
- **Request APIs**: explicit Q_INVOKABLE parameters (e.g. `TranslationService.translate(text, targetLanguage, mode, provider, apiServer, apiKey, model)`) — QVariantMap request bags were removed so arity/type mistakes surface at the call site

### QML Patterns
- **Theme Colors**: Single source of truth is the `ThemeColors` singleton (qml/ThemeColors.qml). Views alias needed tokens as readonly properties; do not hardcode palette hex values.
- **Shared chrome**: Page backgrounds come from `AppBackground`; settings-page dropdowns use `ThemedComboBox`; settings sections are built from `qml/settings/` (SectionCard/SettingsRow + per-domain Section). Extend those components instead of copying their styles.
- **Declarative state**: Avoid imperative assignments that break bindings (e.g. opacity driven by MouseArea state must stay a binding).
- **Window Management**: `ensureWindowVisible()` keeps windows on screen; auto-hide is click-driven (see Window Management gotcha), Esc / ✕ always hide
- **Platform Detection**: `Qt.platform.os` for platform-specific UI adjustments

### Settings Storage
- Location: `QStandardPaths::AppConfigLocation` (`~/.config/TransAI/config.json` on Linux)
- Format: JSON with `configVersion`; per-provider keys (`openaiApiServer/openaiApiKey/openaiModel`, `ollamaApiServer/ollamaModel`) plus active `provider`, `shortCut`, `displayLanguage` (`en` / `zh-CN`; absent/empty falls back to the system UI language), main-view preferences (`targetLanguage` — empty = not set; `translationMode` = 0|1|2 mirroring `TranslationMode`, clamped on load), and screenshot-translation keys (`screenshotShortCut`, `screenshotTranslateMode` = `auto`|`vision`|`ocr`, `screenshotVisionModel` (empty = follow current model), `screenshotMaxImageWidth` = 1600, `screenshotJpegQuality` = 85). Legacy top-level `apiKey/apiServer/model` are READ as one-way migration fallbacks but no longer written; the removed `thinkingMode` key is simply dropped on the next save
- API key secure storage: when an OS keychain backend is available (`SecretStore` — libsecret on Linux when compiled with it, Keychain on macOS, DPAPI on Windows), `openaiApiKey` lives there instead of the file; the config then carries an empty `openaiApiKey` plus the marker `openaiApiKeyInSecretStore: true`. Migration from plaintext is one-way, verified by read-back, and any backend failure keeps the legacy plaintext behavior (the file is chmod 600 on Unix)

### Single-Instance & Selection Translation
- `TransAI --translate-selection` / `--translate-screenshot` / `--background` / plain launch: command forwarded over a local socket (`$XDG_RUNTIME_DIR/transai-command`) to the running instance; first instance becomes the server
- Wayland: QHotkey is disabled; users bind their own desktop hotkey to `TransAI --translate-selection` / `--translate-screenshot`
- X11: selection read via simulated Ctrl+C with clipboard guard; Wayland: `wl-paste --primary` with Qt primary-selection fallback

### Screenshot Translation (multimodal + OCR)
- Flow: trigger (tray/hotkey/CLI) → hide window → `ScreenshotCaptureService` → routed by `screenshotTranslateMode`: the vision path (`VisionTranslationService`) streams into the shared result panel; the OCR path (`OcrEngine` → `TranslationService` with mode forced to Translate) recognizes text first, shows it in the input panel, then streams the translation. Text/vision/OCR sessions abort each other so exactly one owns the panel (`ocrBusy` keeps the status pill honest during recognition)
- Mode routing (`AppView.effectiveScreenshotMode()`): explicit `vision`/`ocr` win; `auto` = vision when the effective screenshot model looks vision-capable, else OCR when an engine is available, else vision (which then surfaces the vision-unsupported guidance)
- OCR backends (`src/ocr/`): tesseract CLI via QProcess on Linux/Windows (optional at runtime — absent binary → `available=false` and the UI says so; language packs probed via `--list-langs`, preferring `chi_sim+eng`), Vision framework (`VNRecognizeTextRequest`, zh-Hans+en) on macOS. QML always sees a valid `ocrEngine` context property
- Wayland capture requires xdg-desktop-portal (system UI handles region selection); X11/Windows/macOS grab every screen and open the in-app fullscreen region-selection overlay (`qml/RegionSelectorWindow.qml`, one per screen; drag = select, Esc/right-click = cancel)
- The vision request reuses provider/server/key/model with the `screenshotVisionModel` override (empty = follow current); image payloads never touch `PromptBuilder`, and QML never sees base64
- Portal handle tokens must be valid D-Bus object-path elements — strip dashes from UUIDs

### API Communication
- **Streaming**: SSE parsing with cross-chunk buffering; `[DONE]` handling; raw-JSON fallback for error bodies
- **Timeouts**: 30s inactivity (transfer timeout, resets while data streams) plus a 5-minute overall cap per request, both in `StreamingChatClient`; the overall-timeout abort deliberately keeps the reply connected so the finished handler emits `translationFailed` (a user `abort()` disconnects first and emits `translationStopped` — the two paths never mix); 15s for model detection
- **Error model**: one terminal signal per request (`translationFinished` / `translationFailed` / `translationStopped`)
- **Thinking suppression**: fully automatic (no user setting), three layers — `ThinkingPolicyBuilder` derives vendor params from provider/host/model id (DeepSeek `thinking.type`, Qwen `enable_thinking`, Ollama `think`, OpenAI `reasoning_effort`) plus `/no_think` and direct-answer prompt hints; `StreamingChatClient` transparently retries once without the params when the endpoint rejects them (subclasses supply a prebuilt `bodyWithoutThinking` in `PendingRequest`); `StreamParser` strips residual `<think>`-style blocks from the streamed content

## Important Gotchas

### Platform-Specific Issues
1. **Linux**: `QT_QUICK_BACKEND=software` is the default (GPU pipeline failures); users can override the env var
2. **Wayland**: global hotkey registration is not available; use the command-server path
3. **macOS**: ApplicationServices framework required for simulated copy
4. **Qt logging on systemd distros**: qDebug output goes to the systemd journal, not stderr; use `journalctl --user` when debugging, or `QT_FORCE_STDERR_LOGGING=1`
5. **Headless platforms (offscreen/minimal/dummy)**: GlobalShortcutManager refuses registration there (QHotkey's X11 backend segfaults); smoke tests log a shortcutRegisterFailed instead of crashing

### Window Management
- The window starts unpinned (not always-on-top); the pin toggle mutates `mainWindow.flags` and opts into always-on-top
- Auto-hide is click-driven, not focus-driven. The real "clicked outside" signal is `GlobalClickMonitor` (passive read-only evdev BTN_LEFT observation, context property `globalClickMonitor`). Telling an outside click from an in-window one is subtle: the `HoverHandler` in `main.qml` lies while an in-window popup (default `Popup.Item`, e.g. the model/provider dropdowns) is open — the popup captures hover, so clicks on it report `hovered=false` at the window root. `main.qml` therefore defers the hide decision ~120 ms after each evdev press (`outsideClickCheckTimer`) and consults `WindowDismissHelper::msecsSinceLastWindowPress()` — an event filter on the root window (`trackWindow()` in main.cpp) that timestamps every press delivered to the window. evdev usually wins the race against the compositor-forwarded window event, hence the deferral; a press paired with a recent in-window press (<250 ms) is never click-outside. This exists because Wayland clients receive NO event for clicks landing on other windows; it needs read access to pointer device nodes (uaccess rule, see docs/hyprland-integration.md) and degrades gracefully without it. Deactivation-driven hides still go through `tryAutoHide()` → `WindowDismissHelper::shouldHideOnDeactivate()`: X11 queries the X server's live button mask via `XQueryPointer` (hides only when the focus loss came from a left-click); Windows/macOS treat any deactivation as a click; Wayland hides only when the deactivation clearly was not caused by the pointer leaving (>300 ms after the last leave). Under focus-follows-mouse compositors (e.g. Hyprland `input:follow_mouse=1`) without evdev access the window survives mouse moves and is dismissed with Esc or the ✕ button

## Testing Strategy

### Current State
- CI (`.github/workflows/ci.yml`) builds on Ubuntu with Qt 6.9 and runs the
  full test suite plus a headless smoke test on every push/PR; Windows
  (MinGW) and macOS jobs run build + tests as non-blocking experimental
  coverage (`continue-on-error` until they prove stable)
- Unit tests live in `tests/` (StreamParser, ThinkingPolicy,
  ProviderEndpointBuilder, I18n, VisionModelCapability, ImagePayloadBuilder,
  Setting persistence incl. secret-store migration with a fake store,
  TranslationHistory persistence, OCR language selection, plus an end-to-end
  StreamingChatClient test against a scripted in-process HTTP/SSE server),
  built when Qt6::Test is available:
  ```bash
  cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_PREFIX_PATH="$QT_PATH" ..
  ctest --test-dir build --output-on-failure
  ```
- UI/backend behavior beyond that is manual, guided by `docs/qa/gates/` when
  applicable
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
- `docs/phase-4-optimization-plan.md` - Post-Phase-3 optimization plan (StreamingChatClient extraction, overall timeout, streaming mock tests, history, Auto target language, geometry persistence, error re-translation, Wayland guidance, Win/macOS CI, SettingView split, SecretStore key storage, OCR loop, typed request APIs all implemented; live-translation debounce intentionally rejected)
- `docs/hyprland-integration.md` - Wayland compositor notes

## Development Workflow

### Git Conventions
- Feature branches; descriptive commit messages in English or Chinese
- Conventional-commit style seen in history: `feat(scope):`, `refactor(scope):`, `docs:`, `style(qml):`

### Code Organization
- C++ backend logic with QML frontend; clear separation between services and UI
- Resources embedded via Qt resource system (TransAI.qrc)
- i18n: UI display language (English / 简体中文) is handled by the `qml/I18n.qml` singleton on top of `Setting.displayLanguage`; wrap user-visible QML strings in `I18n.t(...)` and add Chinese entries to its `_zhTable`. Deliberately outside the Qt Linguist toolchain (lupdate/lrelease are not available in every build environment), so no `.ts`/`.qm` files exist
- Error i18n: fixed English error strings emitted by C++ services double as `_zhTable` keys; QML display points wrap them via `AppView.localizeError()` (composite "<guidance>\n\n<raw>" messages translate only the head; dynamic server payloads stay in English on purpose). AppView keeps the raw English error in `lastRawError` and re-renders it on a live language switch via `I18n.onLanguageChanged`

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

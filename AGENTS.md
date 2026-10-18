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
# Fetch the bundled OCR models first (git-ignored, ~13 MB, pinned checksums)
./scripts/fetch_ocr_models.sh

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
│   ├── TranslationHistory.*        # Recent-translation store (context property "translationHistory"; translation-history.json, newest-first, capped at 100) + cache lookup for repeated requests
│   └── WindowDismissHelper.*       # Click-vs-mouse-move deactivation policy + in-window press timestamps (context property "windowDismiss")
├── input/
│   ├── GlobalShortcutManager.*     # QHotkey-based global hotkeys (QML type; disabled on Wayland); sole authority on which action owns which chord
│   ├── ShortcutConflict.h          # Header-only "one chord = one action" arbitration (unit-tested)
│   ├── GlobalClickMonitor.*        # Passive evdev BTN_LEFT observer for click-outside-to-hide on Wayland (context property; needs /dev/input read access)
│   ├── SelectionTextProvider.*     # Selected-text extraction per platform (QML type)
│   └── ClipboardGuard.*            # Save/restore clipboard around simulated copy
├── providers/
│   ├── ProviderDefaults.h          # Shared provider defaults (server URLs, api-key check)
│   ├── ApiError.h                  # Shared OpenAI-style error-payload message extraction
│   ├── ProviderEndpointBuilder.*   # Chat/models URL normalization per provider (static helpers)
│   ├── NetworkErrorMapper.*        # Shared QNetworkReply error → user-facing message mapping
│   ├── ModelDiscoveryService.*     # /v1/models + ollama /api/tags detection (QML type; 15s transfer + 30s overall cap)
│   └── ThinkingPolicy.*            # Automatic thinking-suppression policy (params + rejection retry)
├── settings/
│   ├── Setting.*                   # config.json load/save + active provider config
│   └── SecretStore*.h/cpp/mm       # OS keychain for the API key (libsecret / Keychain / DPAPI; null fallback)
├── ocr/
│   ├── OcrEngine.h                 # OCR backend interface (context property "ocrEngine") + per-backend factories
│   ├── OcrEngineFactory.cpp        # createOcrEngine(): vision / bundled ncnn / tesseract / unavailable
│   ├── RapidOcrEngine.cpp          # Linux/Windows default: bundled PP-OCR det+rec on ncnn, in-process
│   ├── OcrPostProcess.*            # Pure helpers: score map → boxes, reading order, CTC decode (unit-tested)
│   ├── OcrModelLocator.*           # Pure helper: bundled model file discovery (env/override/app dir/install)
│   ├── TesseractOcrEngine.cpp      # Build-time fallback: tesseract CLI via QProcess
│   ├── VisionOcrEngine.mm          # macOS backend: VNRecognizeTextRequest (Vision framework)
│   └── OcrEngineNull.cpp           # Unavailable fallback backend
├── translation/
│   ├── StreamingChatClient.*       # Shared streaming chat-completion base (SSE, timeouts, thinking retry, terminal signals)
│   ├── TranslationService.*        # Text translation (StreamingChatClient subclass: validation + PromptBuilder body)
│   ├── StreamParser.*              # SSE chunk parser (OpenAI-compatible + raw JSON)
│   ├── TranslationRequest.h        # Request struct (text/target language/provider credentials)
│   └── PromptBuilder.*             # Per-mode system/user prompt construction
├── capture/
│   ├── ScreenshotCaptureService.*  # Unified screenshot entry (QML type): Wayland portal vs QScreen fallback
│   ├── ScreenshotGeometry.h        # Header-only preview-pixel → original-image rect mapping (unit-tested)
│   └── PortalScreenshotBackend.*   # xdg-desktop-portal Screenshot over D-Bus (Linux/Wayland)
├── vision/
│   ├── VisionTranslationService.*  # Multimodal translation (StreamingChatClient subclass: image payload + vision messages + error guidance)
│   ├── ImagePayloadBuilder.*       # QImage → scaled JPEG → base64 data URL (pure logic)
│   └── VisionModelCapability.h     # Header-only vision-model id heuristic (UI hints/routing only)
qml/
├── main.qml                        # Main window, tray, drag, shortcuts, command dispatch
├── AppView.qml                     # Translation interface (session lifecycle + result panel)
├── MainHistoryPopup.qml            # Recent-translations popup (used by AppView)
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
- **Request APIs**: explicit Q_INVOKABLE parameters (e.g. `TranslationService.translate(text, targetLanguage, provider, apiServer, apiKey, model)`) — QVariantMap request bags were removed so arity/type mistakes surface at the call site

### QML Patterns
- **Theme Colors**: Single source of truth is the `ThemeColors` singleton (qml/ThemeColors.qml). Views alias needed tokens as readonly properties; do not hardcode palette hex values.
- **Shared chrome**: Page backgrounds come from `AppBackground`; settings-page dropdowns use `ThemedComboBox`; settings sections are built from `qml/settings/` (SectionCard/SettingsRow + per-domain Section). Extend those components instead of copying their styles.
- **Declarative state**: Avoid imperative assignments that break bindings (e.g. opacity driven by MouseArea state must stay a binding).
- **Main-view sessions**: `AppView` owns one result panel shared by text/vision/OCR; both services forward their signals into `beginSession()` / `updateResultText()` / `showServiceError()` / `finishSession()`. Streamed text is coalesced into a 50ms flush (`pendingResultText`) instead of one TextEdit relayout per SSE chunk, and the history entry is built from the request snapshot (`requestSourceText`/`requestTargetLanguage`), never from the live input field.
- **Repeated-request cache**: the history *is* the cache. `AppView.reuseCachedTranslation()` asks `TranslationHistory::findCachedResult(source, targetLanguage)` (source compared after trimming) before sending anything, and fills the panel from the stored answer without a network request — nothing extra is stored and no duplicate entry is written on a hit; only the text and OCR legs use it, images have no comparable key. History recording is unconditional (no settings switch): every successful translation is stored, the file survives restarts, and the only way to wipe it is the Clear button in the main-view history popup (which also disables the cache). A hit marks `cacheHitKey`, so the status pill reads "From history" and pressing Translate again on that same request is an explicit retranslation (cache bypassed once, fresh answer then replaces the cached entry).
- **Errors**: C++ services map network failures through `NetworkErrorMapper` so model detection and translation share the same wording; server payload messages still win.
- **Accessibility**: icon-only / text-as-button controls carry `Accessible.role` + `Accessible.name` (and `onPressAction` where meaningful).
- **Property naming trap**: in QML, a property whose name is `on` + a capitalized word (`onAccent`) is parsed as a signal handler, not a declaration ("Cannot assign a value to a signal"). Theme tokens therefore use names like `accentForeground`.
- **Global hotkeys**: `GlobalShortcutManager` is the single funnel for every hotkey (`registerShortcut(actionId, sequence)`), and it refuses a chord another action already holds — QHotkey alone accepts a duplicate (it dedups the native grab in its internal multihash) and then fires *every* action bound to that chord on one key press, so the check lives in `input/ShortcutConflict.h` before the platform call. `main.qml` owns one `shortcutRegistrar` (contract: `setShortcut(actionId, shortcut)` -> bool plus `lastError`), the recorders carry their `actionId` (`translateSelection` / `translateScreenshot`), and registration failures are routed to the card that owns the action (`SettingView.hotkeyError` / `screenshotHotkeyError`). Conflict reasons are fixed English sentences so `I18n._zhTable` can translate them.
- **Window Management**: `ensureWindowVisible()` keeps windows on screen; auto-hide is click-driven (see Window Management gotcha), Esc / ✕ always hide
- **Platform Detection**: `Qt.platform.os` for platform-specific UI adjustments

### Settings Storage
- Release packaging requires the bundled OCR models: `scripts/windows-*-publish.ps1`
  throw and `build_install.sh` exits when `res/ocr/models` is incomplete (run
  `scripts/fetch_ocr_models.sh` first; `TRANSAI_SKIP_OCR_MODELS=1` opts out on
  purpose). Shipping an OCR-less artifact silently is the failure mode this
  guards against. The file list and the check live in `scripts/ocr-models.ps1`
  (`Assert-OcrModelsPresent` / `Copy-OcrModelsTo`), dot-sourced by both publish
  scripts, and are covered by the `packaging-dryrun` CI job
  (`workflow_dispatch` only: stubbed build output, Qt runtime and windeployqt,
  so it tests the guard rather than Qt deployment). A dispatch runs that job
  alone — the four build jobs are gated to push/PR
- Shell-script syntax (`build_install.sh`, `scripts/fetch_ocr_models.sh`) is
  parsed by the linux CI job (`bash -n`), because a typo there only bites the
  person running an install by hand
- Location: `QStandardPaths::AppConfigLocation` (`~/.config/TransAI/config.json` on Linux)
- Format: JSON with `configVersion`; per-provider keys (`openaiApiServer/openaiApiKey/openaiModel`, `ollamaApiServer/ollamaModel`) plus active `provider`, `shortCut`, `displayLanguage` (`en` / `zh-CN`; absent/empty falls back to the system UI language), the main-view preference (`targetLanguage` — empty = not set), screenshot-translation keys (`screenshotShortCut`, `screenshotTranslateMode` = `auto`|`vision`|`ocr`, `screenshotVisionModel` (empty = follow current model), `screenshotMaxImageWidth` = 1600, `screenshotJpegQuality` = 85), and window geometry (`windowWidth/windowHeight`; 0 = not set; clamped to bounds shared with QML through `Setting`'s CONSTANT geometry properties). Migrations: `configVersion < 2` configs have their legacy top-level `apiKey/apiServer/model` lifted into the per-provider keys by `migrateLegacyConfig()` (older keys are left on disk for downgrades), a newer `configVersion` only logs a warning, and the removed `thinkingMode` / `historyEnabled` / `clearHistoryOnExit` keys are dropped on the next save (history is always recorded now and never wiped on quit)
- API key secure storage: when an OS keychain backend is available (`SecretStore` — libsecret on Linux when compiled with it, Keychain on macOS, DPAPI on Windows), `openaiApiKey` lives there instead of the file; the config then carries an empty `openaiApiKey` plus the marker `openaiApiKeyInSecretStore: true`. The key is trimmed, migrated from plaintext one-way and verified by read-back; any backend failure keeps the legacy plaintext behavior (the file is chmod 600 on Unix). The keychain is only touched when the key actually changed (`m_lastStoredApiKey`), because `updateConfig()` runs on every settings edit; `Setting.apiKeyStoredSecurely` surfaces the current state in the settings card

### Single-Instance & Selection Translation
- `TransAI --translate-selection` / `--translate-screenshot` / `--background` / plain launch: command forwarded over a local socket (`$XDG_RUNTIME_DIR/transai-command`) to the running instance; first instance becomes the server. `ApplicationCommandServer::start()` refuses to take a socket that still answers (some Qt versions let `listen()` silently rebind an existing path) and only removes a socket file once a connection attempt proved no live instance owns it
- Wayland: QHotkey is disabled; users bind their own desktop hotkey to `TransAI --translate-selection` / `--translate-screenshot`
- X11: selection read via simulated Ctrl+C with clipboard guard; Wayland: `wl-paste --primary` with Qt primary-selection fallback

### Screenshot Translation (multimodal + OCR)
- Flow: trigger (tray/hotkey/CLI) → hide window → `ScreenshotCaptureService` → routed by `screenshotTranslateMode`: the vision path (`VisionTranslationService`) streams into the shared result panel; the OCR path (`OcrEngine` → `TranslationService` with mode forced to Translate) recognizes text first, shows it in the input panel, then streams the translation. The OCR branch stores the recognized text as the session's source, so the history entry keeps both the `[Screenshot]` marker and the text. Text/vision/OCR sessions abort each other so exactly one owns the panel (`ocrBusy` keeps the status pill honest during recognition)
- Mode routing (`AppView.effectiveScreenshotMode()`): explicit `vision`/`ocr` win; `auto` = vision when the effective screenshot model looks vision-capable, else OCR when an engine is available, else vision (which then surfaces the vision-unsupported guidance)
- OCR backends (`src/ocr/`): **bundled RapidOCR** (PP-OCRv3 mobile det+rec on ncnn, in-process, ~13 MB of models fetched by `scripts/fetch_ocr_models.sh` into the git-ignored `res/ocr/models/`) is the default on Linux/Windows; the tesseract CLI backend remains only as a build-time fallback (`-DTRANSAI_BUNDLED_OCR=OFF`); macOS uses the Vision framework (`VNRecognizeTextRequest`, zh-Hans+en). QML always sees a valid `ocrEngine` context property. See `docs/phase-6-bundled-ocr.md` for the pipeline, the size/speed profiles and the known limitations (axis-aligned boxes only, no angle classifier)
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
- The window starts unpinned (not always-on-top); the pin toggle mutates `mainWindow.flags` and opts into always-on-top. The pin button is window-level chrome owned by `main.qml` (id `pinButton`, z above the page stack) and pinned to the window's top-left corner, so it looks and behaves identically on the translation and settings pages and never scrolls away with the settings Flickable; both page headers leave room for it (`AppView` title leftMargin / `SettingView` back-button leftMargin). State lives in `mainWindow.pinned` (single source of truth, written only by `mainWindow.setPinned()`), which the auto-hide logic reads — pages never touch the flag
- Auto-hide is click-driven, not focus-driven. The real "clicked outside" signal is `GlobalClickMonitor` (passive read-only evdev BTN_LEFT observation, context property `globalClickMonitor`). Telling an outside click from an in-window one is subtle: the `HoverHandler` in `main.qml` lies while an in-window popup (default `Popup.Item`, e.g. the model/provider dropdowns) is open — the popup captures hover, so clicks on it report `hovered=false` at the window root. `main.qml` therefore defers the hide decision ~120 ms after each evdev press (`outsideClickCheckTimer`) and consults `WindowDismissHelper::msecsSinceLastWindowPress()` — an event filter on the root window (`trackWindow()` in main.cpp) that timestamps every press delivered to the window. evdev usually wins the race against the compositor-forwarded window event, hence the deferral; a press paired with a recent in-window press (<250 ms) is never click-outside. This exists because Wayland clients receive NO event for clicks landing on other windows; it needs read access to pointer device nodes (uaccess rule, see docs/hyprland-integration.md) and degrades gracefully without it. Deactivation-driven hides still go through `tryAutoHide()` → `WindowDismissHelper::shouldHideOnDeactivate()`: X11 queries the X server's live button mask via `XQueryPointer` (hides only when the focus loss came from a left-click); Windows/macOS treat any deactivation as a click; Wayland hides only when the deactivation clearly was not caused by the pointer leaving (>300 ms after the last leave). Under focus-follows-mouse compositors (e.g. Hyprland `input:follow_mouse=1`) without evdev access the window survives mouse moves and is dismissed with Esc or the ✕ button

## Testing Strategy

### Current State
- CI (`.github/workflows/ci.yml`) builds on Ubuntu with Qt 6.9 and runs the
  full test suite plus a headless smoke test on every push/PR; Windows
  (MinGW) and macOS jobs run build + tests as **blocking** coverage (all
  three platforms gate the build)
- A separate `sanitizers` job rebuilds and runs the whole suite with
  AddressSanitizer + UndefinedBehaviorSanitizer (`detect_leaks=0`)
- The bundled OCR backend pulls ncnn in via FetchContent (CPU-only, SIMD
  profile `portable` by default): the first configure spends ~5–10 min building
  ncnn. `-DTRANSAI_BUNDLED_OCR=OFF` (or `-DTRANSAI_OCR_FETCH_NCNN=OFF` with no
  system ncnn) keeps a build tesseract-only
- The smoke test fails on QML runtime errors (`TypeError`, `ReferenceError`,
  `Unable to assign`, `qrc:/qml/….qml:N:` …), because a broken QML component
  leaves the process alive with an empty window
- Unit tests live in `tests/` (StreamParser, ThinkingPolicy,
  ProviderEndpointBuilder, NetworkErrorMapper, I18n incl. a key-coverage drift
  guard over all QML sources, VisionModelCapability, ImagePayloadBuilder,
  ScreenshotGeometry, Setting persistence incl. secret-store migration/write
  suppression with a fake store + configVersion migration, TranslationHistory
  persistence + cached-result lookup, OCR language selection, ShortcutConflict
  hotkey uniqueness, WindowDismissHelper pairing/offscreen
  policy, ApplicationCommandServer forwarding + stale-socket takeover, plus
  end-to-end StreamingChatClient text and vision request tests against a
  scripted in-process HTTP/SSE server), built when Qt6::Test is available:
  ```bash
  cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_PREFIX_PATH="$QT_PATH" ..
  ctest --test-dir build --output-on-failure
  ```
- QML component tests are not automated yet; the headless smoke test is the
  guard that the whole QML tree instantiates. Other UI behavior stays manual
- Smoke test without a display:
  ```bash
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/TransAI --background
  QT_QPA_PLATFORM=offscreen ./build/TransAI --translate-selection   # from a 2nd process
  ```
- Check QML/engine errors via `journalctl --user` (see logging gotcha above)

### Recommended Test Areas (manual, not yet automated)
1. Provider switch openai ↔ ollama and config persistence
2. Model detection against OpenAI-compatible and Ollama endpoints
3. Hotkey re-registration from the settings page
4. Streaming translation abort (partial result + Stopped status)
5. Real keychain backends (libsecret / Keychain / DPAPI) — untestable in CI:
   no desktop keyring daemon on the runners

## Third-party Components

- `THIRD-PARTY-NOTICES.md` (repository root) lists every bundled/linked
  third-party component with its license (Qt LGPL, QHotkey MIT, ncnn BSD-3,
  PP-OCR models Apache-2.0) and what a release artifact must ship. Update it
  whenever a dependency is added or its pin changes.

## Documentation References

- `docs/phase-1-architecture-refactor.md` - Module split blueprint (implemented)
- `docs/phase-2*.md` - Selection/OCR/UI blueprints (2A/2B implemented; 2C/2D superseded by phase-3/4/5)
- `docs/phase-4-optimization-plan.md` - Post-Phase-3 optimization plan (StreamingChatClient extraction, overall timeout, streaming mock tests, history, Auto target language, geometry persistence, error re-translation, Wayland guidance, Win/macOS CI, SettingView split, SecretStore key storage, OCR loop, typed request APIs all implemented; live-translation debounce intentionally rejected)
- `docs/phase-5-quality-hardening.md` - Post-Phase-4 hardening plan (correctness fixes, AppView split, performance, history privacy, test/CI gaps; all items implemented)
- `docs/phase-6-bundled-ocr.md` - Bundled OCR backend (RapidOCR/PP-OCR on ncnn): pipeline, model sourcing/checksums, install layout, measured size/speed profiles, build options, limitations
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

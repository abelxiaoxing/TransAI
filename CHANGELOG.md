# Changelog

All notable user-visible changes. Dates are release dates; the version in the
binaries comes from `project(TransAI VERSION ...)` in `CMakeLists.txt` (override
per build with `-D APP_VERSION=...`).

## [1.0.0] - unreleased

First tagged version. Development happened in two larger batches (phase 5 and
phase 6 in `docs/`); what users get:

### Translation
- Selection translation on **X11** (simulated copy with clipboard restore) and
  **Wayland** (`wl-paste --primary`, with a Qt primary-selection fallback).
- **Global hotkeys** for selection and screenshot translation (X11/Windows/
  macOS). A chord can only own one action: a duplicate binding is refused with
  an explanation instead of silently firing both. On Wayland the manager is
  disabled; bind `TransAI --translate-selection` /
  `TransAI --translate-screenshot` in the compositor instead.
- **Screenshot translation** with three modes — vision model, OCR, or automatic
  (default): vision when the configured screenshot model looks multimodal,
  otherwise OCR.
- **Bundled OCR**: PP-OCRv3 detection + recognition run in-process on ncnn, so
  OCR works without installing tesseract. Traditional fallback: the tesseract
  CLI when the app is built without the bundled backend.
- **History with cache reuse**: every successful translation is stored (newest
  first, capped at 100). Repeating a request fills the panel from history and
  says "From history"; pressing Translate again forces a fresh translation.
- **Automatic thinking suppression** (DeepSeek/Qwen/Ollama/OpenAI), including a
  transparent retry when an endpoint rejects the vendor parameter.
- **API keys in the OS keychain** where available (libsecret, macOS Keychain,
  Windows DPAPI); plaintext config files are migrated one-way and kept chmod 600.

### Window and UI
- Single instance: a second launch forwards its command (translate selection /
  screenshot / show) to the running instance over a local socket.
- Click-outside-to-hide distinguishes clicks from mouse movement (X11 uses the
  live button mask, Wayland uses a passive evdev observer where permissions
  allow, otherwise Esc/✕ always works).
- Pin/unpin (always-on-top), window size persistence, English/简体中文 UI.

### Packaging
- Windows archives and Linux installs ship the OCR models plus the third-party
  license texts (`licenses/`); packaging refuses to run when either is missing.
- Release builds are reproducible from CI: `.github/workflows/release.yml`
  (`workflow_dispatch`) produces `TransAI-<tag>-linux-x86_64.tar.gz` and
  `TransAI-<tag>-win64.zip`.

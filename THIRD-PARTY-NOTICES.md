# Third-party notices

TransAI bundles and links third-party components. This file records what they
are, where they enter the build, and under which license, so that release
artifacts can ship the required attribution. It covers third-party components
only; the project itself has no license file yet.

| Component | Version / pin | License | How it enters the build |
|---|---|---|---|
| [Qt](https://www.qt.io/) 6 (qtbase, qtdeclarative: Core, Gui, Quick, Network, DBus, Concurrent, Test) | 6.9.x (see CI / `CMakeLists.txt`) | LGPL-3.0-only **or** commercial Qt license | Used as shared libraries, dynamically linked |
| [ICU](https://icu.unicode.org/) | as shipped with the Qt binaries | Unicode License v3 | Transitive dependency of the prebuilt Qt libraries on Linux |
| [QHotkey](https://github.com/Skycoder42/QHotkey) | 1.5.0 | BSD-3-Clause (upstream moved to MIT *after* 1.5.0; the vendored tag is BSD-3) | Vendored source in `lib/QHotkey-1.5.0/`, linked statically |
| [ncnn](https://github.com/Tencent/ncnn) | `TRANSAI_NCNN_GIT_TAG` (default `20260526`) | BSD-3-Clause (plus third-party components listed in ncnn's `LICENSE.txt`) | Fetched by CMake `FetchContent` and linked statically into the OCR backend |
| [PP-OCRv3](https://github.com/PaddlePaddle/PaddleOCR) models, as redistributed by [RapidOCR](https://github.com/RapidAI/RapidOcrNcnn) | RapidOcrNcnn 1.2.0 archive, SHA256-pinned in `scripts/fetch_ocr_models.sh` | Apache-2.0 | Model files (`res/ocr/models/`, **not** in git) downloaded at build time and shipped next to the app |
| [libsecret](https://gitlab.gnome.org/GNOME/libsecret) | distribution package | LGPL-2.1-or-later | Optional, detected at configure time on Linux; linked dynamically |

## What a release artifact must include

The texts themselves are vendored in `LICENSES/` (see `LICENSES/README.md`) and
are shipped as `licenses/` in every artifact — CMake's install rule on
Linux/macOS, `scripts/release-assets.ps1` (`Copy-LicenseFiles`) in the Windows
publish scripts. Packaging aborts when a text is missing or empty, so a release
cannot silently ship without them.

1. **Qt** — LGPL compliance: ship the dynamically linked Qt libraries, and state
   in the about/documentation that Qt is used under the LGPL with a link to
   <https://www.qt.io/licensing/open-source-lgpl-obligations>. Users must be able
   to relink against a modified Qt (shipping `.so`/`.dll` files next to the
   executable, as `windeployqt` does, satisfies this).
2. **QHotkey and ncnn (BSD-3-Clause)** — statically linked, so the texts must
   travel with the artifact: `licenses/QHotkey-BSD-3-Clause.txt`,
   `licenses/ncnn-BSD-3-Clause.txt`. Refresh them from the pinned tags when a pin
   changes.
3. **PP-OCR models (Apache-2.0)** — `licenses/PP-OCR-Apache-2.0.txt`, plus the
   attribution to PaddleOCR/RapidOCR in this file.
4. **libsecret (LGPL-2.1)** — linked dynamically; no action beyond naming it.

## Notes

- The model files are deliberately kept out of git (13 MB of binaries) and
  fetched with pinned SHA256 digests by `scripts/fetch_ocr_models.sh`, so a
  build without the models is fully functional except for OCR mode (which
  reports itself unavailable). Packaging scripts refuse to run without them —
  see `scripts/windows-*.ps1` and `build_install.sh`.
- ncnn is built with `NCNN_VULKAN=OFF`, no tools/benchmarks/tests and no
  external submodules, so the third-party components listed inside ncnn's own
  `LICENSE.txt` reduce to its bundled zlib-licensed pieces.
- See `docs/phase-6-bundled-ocr.md` for the OCR pipeline and why ncnn is
  bundled instead of relying on a system OCR installation.

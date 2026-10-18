# License texts shipped with TransAI

The packaging step copies this directory into the release artifact as
`licenses/` (CMake install rule on Linux/macOS, the Windows publish scripts for
the zips). `THIRD-PARTY-NOTICES.md` in the repository root explains which
component each file covers and what the obligations are.

| File | Covers |
|---|---|
| `QHotkey-BSD-3-Clause.txt` | QHotkey 1.5.0, vendored in `lib/QHotkey-1.5.0/` (note: upstream switched to MIT *after* 1.5.0 — the text below is the one that applies to the vendored tag) |
| `ncnn-BSD-3-Clause.txt` | ncnn, fetched by CMake at the pinned `TRANSAI_NCNN_GIT_TAG` |
| `PP-OCR-Apache-2.0.txt` | PP-OCRv3 weights redistributed by RapidOCR, fetched by `scripts/fetch_ocr_models.sh` |
| `Qt-LGPL-3.0.txt`, `GPL-3.0.txt` | Qt 6 shared libraries (LGPL-3.0 incorporates GPL-3.0 by reference) |

When a dependency is added or its pin changes, refresh the corresponding file
here — the texts are vendored on purpose so packaging never needs the network and
a release cannot silently ship without them.

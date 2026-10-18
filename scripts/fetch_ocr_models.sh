#!/usr/bin/env bash
#
# Fetches the PP-OCR models used by the bundled OCR backend into
# res/ocr/models (git-ignored: they are ~13 MB of binary weights).
#
#   ./scripts/fetch_ocr_models.sh
#
# Source: RapidOcrNcnn 1.2.0 release archive (Apache-2.0; the models are
# PaddleOCR's PP-OCRv3 mobile det/rec exported to ncnn). Only the detection,
# recognition and dictionary files are extracted — the angle classifier is not
# used by TransAI (screenshot text is horizontal), which keeps ~0.5 MB and one
# inference per line out of the picture.
#
# The extracted files are verified against pinned SHA256 digests, so a
# corrupted or substituted download fails loudly instead of silently degrading
# recognition quality.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEST_DIR="${1:-$ROOT_DIR/res/ocr/models}"

ARCHIVE_URL="https://github.com/RapidAI/RapidOcrNcnn/releases/download/1.2.0/Project_RapidOcrNcnn-1.2.0.7z"
ARCHIVE_SHA256="73f3b8d4d819cb0482ec21e3e232100844250dbff440f1bb47adbe3e94c056f5"
ARCHIVE_PREFIX="Project_RapidOcrNcnn-1.2.0/models"

# name:sha256 pairs, verified after extraction.
FILES=(
    "ch_PP-OCRv3_det_infer.param:2e4701d9c9fa6aa1864067683e8357991496459234c7ea73b253d8e5fcc121c6"
    "ch_PP-OCRv3_det_infer.bin:9c233d604bbb4a6e6fc1db34d3890cf5eb4de8d71167072d1ec0fc3691c665f7"
    "ch_PP-OCRv3_rec_infer.param:4158faa4fa1a5c60d1c8ff510765a52c77abf77aeda6c1608f2ba08d3542704e"
    "ch_PP-OCRv3_rec_infer.bin:4c5ac2ac7ffb208109fd78ded6f89d43850b721eb414a058af1dda2fe6994085"
    "ppocr_keys_v1.txt:28b2362ad4ab2dc38769aa72feb535e3a9ddb3fd2a7585a05920e6393b1dc7f7"
)

have_all_files() {
    local entry name
    for entry in "${FILES[@]}"; do
        name="${entry%%:*}"
        [[ -f "$DEST_DIR/$name" ]] || return 1
    done
    return 0
}

verify_files() {
    local entry name expected actual
    for entry in "${FILES[@]}"; do
        name="${entry%%:*}"
        expected="${entry##*:}"
        actual="$(sha256sum "$DEST_DIR/$name" | cut -d' ' -f1)"
        if [[ "$actual" != "$expected" ]]; then
            echo "error: $name has an unexpected checksum" >&2
            echo "  expected $expected" >&2
            echo "  actual   $actual" >&2
            return 1
        fi
    done
    echo "OCR models verified in $DEST_DIR"
}

if have_all_files && verify_files; then
    exit 0
fi

extractor=""
for candidate in 7z 7za 7zr; do
    if command -v "$candidate" >/dev/null 2>&1; then
        extractor="$candidate"
        break
    fi
done
if [[ -z "$extractor" ]]; then
    echo "error: no 7-Zip extractor found (install p7zip or 7zip)" >&2
    exit 1
fi

mkdir -p "$DEST_DIR"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

echo "Downloading RapidOcrNcnn 1.2.0 model archive (~69 MB)..."
curl -L --fail --retry 3 -o "$tmp_dir/models.7z" "$ARCHIVE_URL"

echo "Verifying archive checksum..."
echo "$ARCHIVE_SHA256  $tmp_dir/models.7z" | sha256sum -c -

echo "Extracting models..."
"$extractor" e -y -o"$DEST_DIR" "$tmp_dir/models.7z" "$ARCHIVE_PREFIX/*" >/dev/null

# The archive also carries the angle classifier, which TransAI does not use.
rm -f "$DEST_DIR/ch_ppocr_mobile_v2.0_cls_infer.param" \
      "$DEST_DIR/ch_ppocr_mobile_v2.0_cls_infer.bin" \
      "$DEST_DIR/README.md"

verify_files
du -sh "$DEST_DIR"

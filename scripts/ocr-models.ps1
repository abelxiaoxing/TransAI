# Shared OCR model handling for the packaging scripts.
#
# The bundled PP-OCR weights live in res/ocr/models (see
# scripts/fetch_ocr_models.sh); they are not in git, so a packaging run on a
# fresh checkout can silently produce an archive whose OCR mode never works.
# Both publish scripts dot-source this file and call Copy-OcrModelsTo, which
# aborts the packaging run when a model file is missing.
#
# Tested by the "packaging-dryrun" job in .github/workflows/ci.yml
# (workflow_dispatch only).

# Files OcrModelLocator::findModelDir() needs; keep in sync with
# src/ocr/OcrModelLocator.cpp and scripts/fetch_ocr_models.sh.
$script:OcrModelFiles = @(
    'ch_PP-OCRv3_det_infer.param',
    'ch_PP-OCRv3_det_infer.bin',
    'ch_PP-OCRv3_rec_infer.param',
    'ch_PP-OCRv3_rec_infer.bin',
    'ppocr_keys_v1.txt'
)

function Assert-OcrModelsPresent {
    param(
        [string] $ModelDir = (Join-Path (Get-Location) 'res\ocr\models')
    )
    $missing = $script:OcrModelFiles | Where-Object {
        -not (Test-Path (Join-Path $ModelDir $_))
    }
    if ($missing) {
        throw ("{0} is incomplete (missing: {1}). Run ./scripts/fetch_ocr_models.sh before packaging." -f $ModelDir, ($missing -join ', '))
    }
}

function Copy-OcrModelsTo {
    param(
        [Parameter(Mandatory = $true)][string] $Destination,
        [string] $ModelDir = (Join-Path (Get-Location) 'res\ocr\models')
    )
    Assert-OcrModelsPresent -ModelDir $ModelDir
    New-Item -ItemType Directory -Force $Destination | Out-Null
    Copy-Item (Join-Path $ModelDir '*') $Destination
}

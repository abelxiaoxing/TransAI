# Shared release-asset handling for the packaging scripts.
#
# The third-party license texts live in LICENSES/ (see LICENSES/README.md) and
# have to travel with every archive: QHotkey/ncnn are statically linked, the
# PP-OCR weights are redistributed, and Qt is linked under the LGPL. Missing
# texts are a compliance bug, so packaging aborts rather than shipping without
# them.
#
# Tested by the "packaging-dryrun" job in .github/workflows/ci.yml
# (workflow_dispatch only).

$script:LicenseFiles = @(
    'QHotkey-BSD-3-Clause.txt',
    'ncnn-BSD-3-Clause.txt',
    'PP-OCR-Apache-2.0.txt',
    'Qt-LGPL-3.0.txt',
    'GPL-3.0.txt'
)

function Copy-LicenseFiles {
    param(
        [Parameter(Mandatory = $true)][string] $Destination,
        [string] $LicenseDir = (Join-Path (Get-Location) 'LICENSES')
    )
    $missing = $script:LicenseFiles | Where-Object {
        $path = Join-Path $LicenseDir $_
        -not (Test-Path $path) -or (Get-Item $path).Length -eq 0
    }
    if ($missing) {
        throw ("{0} is missing license texts (missing or empty: {1}). See LICENSES/README.md." -f $LicenseDir, ($missing -join ', '))
    }
    New-Item -ItemType Directory -Force $Destination | Out-Null
    Copy-Item (Join-Path $LicenseDir '*') $Destination
}

# Shared release-asset handling for the packaging scripts.
#
# The third-party license texts live in LICENSES/ (see LICENSES/README.md) and
# have to travel with every archive: QHotkey/ncnn are statically linked, the
# PP-OCR weights are redistributed, Qt is linked under the LGPL and the OpenMP
# runtime (libgomp/vcomp) is an OpenMP-enabled ncnn dependency. Missing texts are
# a compliance bug, so packaging aborts rather than shipping without them.
#
# Tested by the "packaging-dryrun" job in .github/workflows/ci.yml
# (workflow_dispatch only).

$script:LicenseFiles = @(
    'QHotkey-BSD-3-Clause.txt',
    'ncnn-BSD-3-Clause.txt',
    'PP-OCR-Apache-2.0.txt',
    'Qt-LGPL-3.0.txt',
    'GPL-3.0.txt',
    'GCC-Runtime-Library-Exception-3.1.txt'
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

# True when the built executable actually imports $DllName. Used for the OpenMP
# runtime (libgomp-1.dll on MinGW, vcomp140.dll on MSVC): the bundled OCR backend
# links ncnn with OpenMP (CMakeLists `NCNN_OPENMP`), and a missing runtime means
# the app dies at launch with no OCR at all — but a build without OpenMP must not
# be blocked by a check for a DLL it never needs. The PE import table stores the
# DLL name as plain ASCII, so scanning the file avoids depending on
# dumpbin/objdump being on PATH.
function Test-BinaryImportsDll {
    param(
        [Parameter(Mandatory = $true)][string] $Path,
        [Parameter(Mandatory = $true)][string] $DllName
    )
    $text = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($Path))
    # Case-insensitive on purpose: a missed import ships a zip that crashes at
    # launch, while a stray match only copies a DLL nobody loads. Import names
    # are lowercase in practice (the linker copies them from the import lib),
    # so this is belt and braces. OrdinalIgnoreCase needs .NET Core, which the
    # pwsh runners and developer machines all have.
    return $text.Contains($DllName, [System.StringComparison]::OrdinalIgnoreCase)
}

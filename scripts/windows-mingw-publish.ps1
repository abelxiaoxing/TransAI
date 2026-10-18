[CmdletBinding()]
param (
    [string] $archiveName, [string] $targetName, [string] $Qt6_DIR
)
# 外部环境变量包括:
# archiveName: ${{ matrix.qt_ver }}-${{ matrix.qt_arch }}


# archiveName: 5.15.2-win64_mingw81

. "$PSScriptRoot/ocr-models.ps1"
. "$PSScriptRoot/release-assets.ps1"

$scriptDir = $PSScriptRoot
$currentDir = Get-Location
Write-Host "currentDir" $currentDir
Write-Host "scriptDir" $scriptDir

function Main() {

    New-Item -ItemType Directory $archiveName
    # 拷贝exe
    Copy-Item build\* $archiveName\
    # 拷贝依赖
    windeployqt --qmldir . --plugindir $archiveName\plugins --no-translations --compiler-runtime $archiveName\$targetName
    # 内置 OCR 模型（见 scripts/fetch_ocr_models.sh）。缺失就中止打包：
    # 打出一个 OCR 永远不可用的包比打不出包更难排查。
    Copy-OcrModelsTo -Destination "$archiveName\ocr\models"
    # 第三方许可文本（见 LICENSES/README.md 与 THIRD-PARTY-NOTICES.md）：
    # 与模型文件同等重要，缺失同样中止打包。
    Copy-LicenseFiles -Destination "$archiveName\licenses"
    # 编译器运行时：windeployqt --compiler-runtime 已经从工具链里拷过一遍，
    # 这里只补它漏掉的。Qt 6.9 的 mingw_64\bin 里没有这三个 DLL（它们在
    # Tools\mingw*_64\bin），所以按"Qt bin → PATH → 已在包里"的顺序找，都找不到
    # 才报错，而不是像以前那样直接 cp 一个不存在的路径。
    foreach ($dll in @('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
        $source = Join-Path $Qt6_DIR "bin\$dll"
        if (-not (Test-Path $source)) {
            $onPath = Get-Command $dll -ErrorAction SilentlyContinue
            if ($onPath) { $source = $onPath.Source }
        }
        if (Test-Path $source) {
            Copy-Item $source $archiveName
        } elseif (-not (Test-Path (Join-Path $archiveName $dll))) {
            throw ("runtime {0} is missing from the archive and was not found under {1}\bin" -f $dll, $Qt6_DIR)
        }
    }
    # 删除不必要的文件
    $excludeList = @("*.qmlc", "*.ilk", "*.exp", "*.lib", "*.pdb")
    Remove-Item -Path $archiveName -Include $excludeList -Recurse -Force
    # 打包zip
    Compress-Archive -Path $archiveName $archiveName'.zip'
}

if ($null -eq $archiveName || $null -eq $targetName) {
    Write-Host "args missing, archiveName is" $archiveName ", targetName is" $targetName
    return
}
Main

[CmdletBinding()]
param (
    [string] $archiveName, [string] $targetName
)
# 外部环境变量包括:
# archiveName: ${{ matrix.qt_ver }}-${{ matrix.qt_arch }}
# winSdkDir: ${{ steps.build.outputs.winSdkDir }}
# winSdkVer: ${{ steps.build.outputs.winSdkVer }}
# vcToolsInstallDir: ${{ steps.build.outputs.vcToolsInstallDir }}
# vcToolsRedistDir: ${{ steps.build.outputs.vcToolsRedistDir }}
# msvcArch: ${{ matrix.msvc_arch }}


# winSdkDir: C:\Program Files (x86)\Windows Kits\10\ 
# winSdkVer: 10.0.19041.0\ 
# vcToolsInstallDir: C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Tools\MSVC\14.28.29333\ 
# vcToolsRedistDir: C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Redist\MSVC\14.28.29325\ 
# archiveName: 5.9.9-win32_msvc2015
# msvcArch: x86

. "$PSScriptRoot/ocr-models.ps1"
. "$PSScriptRoot/release-assets.ps1"

$scriptDir = $PSScriptRoot
$currentDir = Get-Location
Write-Host "currentDir" $currentDir
Write-Host "scriptDir" $scriptDir

function Main() {

    New-Item -ItemType Directory $archiveName
    # 只拷产物：Copy-Item build\* 会把 CMakeFiles/、_deps/（ncnn 源码与中间对象）
    # 一起打进包，实测一个包里大半体积是这些。（发布流程实测 44MB → 修完后 ~30MB）
    $exe = @((Join-Path 'build' $targetName), (Join-Path 'build' "$targetName.exe")) |
        Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $exe) {
        throw ("no build output '{0}' under build\; run the Release build first" -f $targetName)
    }
    Copy-Item $exe $archiveName\
    Get-ChildItem build -Filter *.dll -File | Copy-Item -Destination $archiveName
    # 拷贝依赖
    # windeployqt 需要具体文件路径：用解析出来的 exe 文件名，这样 -targetName
    # 传 "TransAI" 或 "TransAI.exe" 都能工作。
    $exeLeaf = Split-Path $exe -Leaf
    windeployqt --qmldir . --plugindir $archiveName\plugins --no-translations --compiler-runtime (Join-Path $archiveName $exeLeaf)
    # 删除不必要的文件
    $excludeList = @("*.qmlc", "*.ilk", "*.exp", "*.lib", "*.pdb")
    Remove-Item -Path $archiveName -Include $excludeList -Recurse -Force
    # 内置 OCR 模型（见 scripts/fetch_ocr_models.sh）。缺失就中止打包：
    # 打出一个 OCR 永远不可用的包比打不出包更难排查。
    Copy-OcrModelsTo -Destination "$archiveName\ocr\models"
    # 第三方许可文本（见 LICENSES/README.md 与 THIRD-PARTY-NOTICES.md）：
    # 与模型文件同等重要，缺失同样中止打包。
    Copy-LicenseFiles -Destination "$archiveName\licenses"
    # 拷贝vcRedist dll
    $redistDll="{0}{1}\*.CRT\*.dll" -f $env:vcToolsRedistDir.Trim(),$env:msvcArch
    Copy-Item $redistDll $archiveName\
    # 拷贝WinSDK dll
    $sdkDll="{0}Redist\{1}ucrt\DLLs\{2}\*.dll" -f $env:winSdkDir.Trim(),$env:winSdkVer.Trim(),$env:msvcArch
    Copy-Item $sdkDll $archiveName\
    # 打包zip
    Compress-Archive -Path $archiveName $archiveName'.zip'
}

if ($null -eq $archiveName || $null -eq $targetName) {
    Write-Host "args missing, archiveName is" $archiveName ", targetName is" $targetName
    return
}
Main


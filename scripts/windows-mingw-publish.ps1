[CmdletBinding()]
param (
    [string] $archiveName, [string] $targetName, [string] $Qt6_DIR
)
# 外部环境变量包括:
# archiveName: ${{ matrix.qt_ver }}-${{ matrix.qt_arch }}


# archiveName: 5.15.2-win64_mingw81

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
    # Bundled OCR models (see scripts/fetch_ocr_models.sh); the app resolves
    # them from <exe dir>\ocr\models.
    if (Test-Path res\ocr\models) {
        New-Item -ItemType Directory -Force "$archiveName\ocr\models" | Out-Null
        Copy-Item res\ocr\models\* "$archiveName\ocr\models\"
    } else {
        Write-Warning "res/ocr/models missing: run scripts/fetch_ocr_models.sh (OCR mode will be unavailable)"
    }
    cp $Qt6_DIR\bin\libgcc_s_seh-1.dll $archiveName\
    cp $Qt6_DIR\bin\libstdc++-6.dll $archiveName\
    cp $Qt6_DIR\bin\libwinpthread-1.dll $archiveName\
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

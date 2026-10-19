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
    # libgomp-1.dll 是 OpenMP 运行时（内置 OCR 的 ncnn 并行靠它，见 CMakeLists
    # 的 NCNN_OPENMP）：只在 exe 真的导入它时才必需，漏了就是启动即崩。
    $runtimeDlls = @('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')
    if (Test-BinaryImportsDll -Path $exe -DllName 'libgomp-1.dll') {
        $runtimeDlls += 'libgomp-1.dll'
    }
    foreach ($dll in $runtimeDlls) {
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

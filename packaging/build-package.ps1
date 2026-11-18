<#
.SYNOPSIS
    构建「摸鱼记账」Release 版本、部署 Qt 运行时、可选精简，并打成便携 ZIP（可选生成安装包）。

.DESCRIPTION
    流程：CMake Release 构建 → cmake --install 到 dist\MoyuLedger（含 windeployqt）
          → 精简（翻译/调试插件；-Slim 再加删软件渲染与多余样式）
          → Compress-Archive 成便携包 → 若找到 Inno Setup 的 ISCC.exe，额外编译安装包。

.PARAMETER Slim
    额外删除 opengl32sw.dll（软件 OpenGL 回退，约 20MB）和 D3Dcompiler_47.dll。
    仅在确定所有目标机器都有可用显卡驱动时使用。

.PARAMETER QtPath
    Qt 安装前缀，默认 C:\Qt\6.11.1\mingw_64。

.PARAMETER Version
    版本号，默认 1.0.0。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File packaging\build-package.ps1
    powershell -ExecutionPolicy Bypass -File packaging\build-package.ps1 -Slim
#>
param(
    [switch]$Slim,

    # 跳过 Inno Setup 安装包（CI 或没装中文语言包时用）
    [switch]$NoInstaller,
    [string]$QtPath = "C:\Qt\6.11.1\mingw_64",
    [string]$Version = "1.0.0",

    # 代码签名（可选）：给 PFX 证书即可同时签 exe 与安装包
    [string]$SignCert = "",
    [string]$SignPassword = "",
    [string]$SignTimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\release"
$distDir = Join-Path $root "dist\MoyuLedger"
$distRoot = Join-Path $root "dist"

$env:PATH = "$QtPath\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin;" + $env:PATH

# 找 signtool（Windows SDK）
function Find-SignTool {
    $candidates = @()
    $sdkRoot = "C:\Program Files (x86)\Windows Kits\10\bin"
    if (Test-Path $sdkRoot) {
        $candidates = Get-ChildItem $sdkRoot -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName "x64\signtool.exe" } |
            Where-Object { Test-Path $_ }
    }
    $cmd = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($cmd) { $candidates += $cmd.Source }
    return $candidates | Select-Object -First 1
}

$signTool = $null
if ($SignCert) {
    $signTool = Find-SignTool
    if (-not $signTool) {
        Write-Host "    未找到 signtool.exe（需安装 Windows SDK），跳过签名" -ForegroundColor Yellow
    } elseif (-not (Test-Path $SignCert)) {
        Write-Host "    证书文件不存在：$SignCert，跳过签名" -ForegroundColor Yellow
        $signTool = $null
    }
}

# Qt 的 windeployqt / CMake 会往 stderr 写无害警告；这里不让它中断脚本
$previousEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"

Write-Host "==> [1/5] CMake 配置（Release）" -ForegroundColor Cyan
cmake -S $root -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtPath" 2>&1 |
    Where-Object { $_ -notmatch "WrapVulkanHeaders" } | Out-Null
if ($LASTEXITCODE -ne 0) { $ErrorActionPreference = $previousEap; throw "CMake 配置失败" }

Write-Host "==> [2/5] 编译" -ForegroundColor Cyan
cmake --build $buildDir
if ($LASTEXITCODE -ne 0) { throw "编译失败" }

Write-Host "==> [3/5] 部署到 dist\MoyuLedger（windeployqt）" -ForegroundColor Cyan
if (Test-Path $distDir) { Remove-Item $distDir -Recurse -Force }
cmake --install $buildDir --prefix $distDir 2>&1 |
    Where-Object { $_ -notmatch "dxcompiler|dxil\.dll" } | Out-Null
if ($LASTEXITCODE -ne 0) { $ErrorActionPreference = $previousEap; throw "部署失败" }
$ErrorActionPreference = $previousEap
if (-not (Test-Path (Join-Path $distDir "MoyuLedger.exe"))) { throw "部署产物缺少 MoyuLedger.exe" }

if ($signTool) {
    Write-Host "==> 签名主程序" -ForegroundColor Cyan
    & $signTool sign /f $SignCert /p $SignPassword /fd sha256 /tr $SignTimestampUrl /td sha256 (Join-Path $distDir "MoyuLedger.exe") | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "    exe 签名失败（继续打包）" -ForegroundColor Yellow }
    else { Write-Host "    已签名 MoyuLedger.exe" -ForegroundColor Green }
}

Write-Host "==> [4/5] 精简" -ForegroundColor Cyan
$translations = Join-Path $distDir "translations"
if (Test-Path $translations) {
    Get-ChildItem $translations -Filter *.qm |
        Where-Object { $_.BaseName -notin @("qt_zh_CN", "qt_en") } |
        Remove-Item -Force -ErrorAction SilentlyContinue
}
Remove-Item (Join-Path $distDir "qmltooling") -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $distDir "translations\qt_help*.qm") -Force -ErrorAction SilentlyContinue
if ($Slim) {
    foreach ($name in @("opengl32sw.dll", "D3Dcompiler_47.dll")) {
        Remove-Item (Join-Path $distDir $name) -Force -ErrorAction SilentlyContinue
    }
    Write-Host "    已启用 -Slim：移除软件 OpenGL 与 D3D 编译器" -ForegroundColor Yellow
}

$files = Get-ChildItem $distDir -Recurse -File
$sizeMb = [math]::Round((($files | Measure-Object Length -Sum).Sum / 1MB), 1)
Write-Host ("    部署完成：{0} 个文件，{1} MB" -f $files.Count, $sizeMb)

Write-Host "==> [5/5] 打包" -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
$zipPath = Join-Path $distRoot "MoyuLedger-$Version-portable.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path (Join-Path $distDir "*") -DestinationPath $zipPath -CompressionLevel Optimal
$zipMb = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
Write-Host ("    便携包：{0}（{1} MB）" -f $zipPath, $zipMb) -ForegroundColor Green

$isccCandidates = @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ }
$iscc = $isccCandidates | Select-Object -First 1
if (-not $iscc) {
    $cmd = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($cmd) { $iscc = $cmd.Source }
}

if ($iscc -and -not $NoInstaller) {
    Write-Host "==> 生成安装包（Inno Setup）" -ForegroundColor Cyan
    $isccArgs = @("/DMyVersion=$Version")
    if ($signTool) {
        $signCommand = ('"{0}" sign /f "{1}" /p {2} /fd sha256 /tr {3} /td sha256 $f' -f $signTool, $SignCert, $SignPassword, $SignTimestampUrl)
        $isccArgs += "/DSignCommand=1"
        $isccArgs += "/Skeep_sign=$signCommand"
    }
    & $iscc @isccArgs (Join-Path $PSScriptRoot "installer.iss")
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup 编译失败" }
    Get-ChildItem $distRoot -Filter "*-setup.exe" | ForEach-Object {
        Write-Host ("    安装包：{0}（{1} MB）" -f $_.FullName, [math]::Round($_.Length / 1MB, 1)) -ForegroundColor Green
    }
} else {
    if ($NoInstaller) { Write-Host "    已跳过安装包（-NoInstaller）" -ForegroundColor Yellow }
    else { Write-Host "    未找到 Inno Setup（ISCC.exe），跳过安装包；便携包已生成。" -ForegroundColor Yellow }
    Write-Host "    安装 Inno Setup 6 后重跑本脚本即可自动产出 *-setup.exe。" -ForegroundColor Yellow
}
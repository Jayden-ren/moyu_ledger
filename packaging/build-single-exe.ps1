<#
.SYNOPSIS
    构建「摸鱼记账」单文件可执行程序（内嵌便携包 + 启动器）。

.DESCRIPTION
    1) 调用 build-package.ps1 生成 Release 便携包（dist\MoyuLedger-<版本>-portable.zip）
    2) 用 windres 把便携包编译成 exe 的 RCDATA 资源
    3) 用 MinGW 编译 Win32 启动器并链接该资源 → dist\摸鱼记账-<版本>-单文件.exe
    首次运行会把内置包解压到 %LOCALAPPDATA%\MoyuLedger\app-<版本>，
    并用独立数据目录 %LOCALAPPDATA%\MoyuLedger\fresh 启动（开箱即空）。

.PARAMETER Version
    版本号，默认 1.0.0

.PARAMETER SkipPackage
    跳过便携包重建（dist 下已有 zip 时加速）

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File packaging\build-single-exe.ps1
#>
param(
    [string]$Version = "1.0.0",
    [switch]$SkipPackage
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$mingwBin = "C:\Qt\Tools\mingw1310_64\bin"
$windres = Join-Path $mingwBin "windres.exe"
$gxx = Join-Path $mingwBin "g++.exe"

if (-not $SkipPackage) {
    Write-Host "==> [1/4] 生成便携包" -ForegroundColor Cyan
    & powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "build-package.ps1") -Version $Version
    if ($LASTEXITCODE -ne 0) { throw "便携包构建失败" }
} else {
    Write-Host "==> [1/4] 跳过便携包构建" -ForegroundColor Yellow
}

$zipPath = Join-Path $root "dist\MoyuLedger-$Version-portable.zip"
if (-not (Test-Path $zipPath)) { throw "找不到便携包：$zipPath" }
$zipMb = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
Write-Host ("    便携包：{0}（{1} MB）" -f $zipPath, $zipMb)

$workDir = Join-Path $root "build\launcher"
New-Item -ItemType Directory -Force -Path $workDir | Out-Null

# 版本号用生成的头文件传入，避免命令行引号转义问题
$versionHeader = Join-Path $workDir "version.h"
[System.IO.File]::WriteAllText($versionHeader,
    "#pragma once`n#define APP_VERSION_W L`"$Version`"`n",
    (New-Object System.Text.UTF8Encoding($false)))

Write-Host "==> [2/4] 生成资源文件（内嵌便携包）" -ForegroundColor Cyan
$rcPath = Join-Path $workDir "app.rc"
$rcContent = @"
1 RCDATA "$($zipPath -replace '\\', '\\')"
"@
[System.IO.File]::WriteAllText($rcPath, $rcContent, (New-Object System.Text.UTF8Encoding($false)))

$resPath = Join-Path $workDir "app.res"
& $windres -i $rcPath -o $resPath -O coff
if ($LASTEXITCODE -ne 0) { throw "windres 编译资源失败" }

Write-Host "==> [3/4] 编译启动器" -ForegroundColor Cyan
$outExe = Join-Path $root "dist\摸鱼记账-$Version-单文件.exe"
& $gxx -O2 -s -mwindows -municode -DUNICODE -D_UNICODE -DKEEP_VERSION_HEADER `
    "-I$workDir" `
    (Join-Path $PSScriptRoot "launcher\main.cpp") $resPath `
    -o $outExe -lshell32
if ($LASTEXITCODE -ne 0) { throw "启动器编译失败" }

Write-Host "==> [4/4] 完成" -ForegroundColor Cyan
$exeMb = [math]::Round((Get-Item $outExe).Length / 1MB, 1)
Write-Host ("    单文件程序：{0}（{1} MB）" -f $outExe, $exeMb) -ForegroundColor Green
Write-Host "    首次运行解压到 %LOCALAPPDATA%\MoyuLedger\app-$Version（约 120 MB），" -ForegroundColor Gray
Write-Host "    数据目录 %LOCALAPPDATA%\MoyuLedger\fresh（开箱即空）。" -ForegroundColor Gray
Write-Host "    想使用已有账本数据：摸鱼记账-$Version-单文件.exe --use-default-data" -ForegroundColor Gray
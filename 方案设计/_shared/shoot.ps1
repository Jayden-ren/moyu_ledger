<#
  把 1440x900 的设计稿 HTML 截成 PNG（本机为 Windows PowerShell 5.1，用 powershell.exe 调用）。

  用法：
    powershell -NoProfile -ExecutionPolicy Bypass -File "<项目>\方案设计\_shared\shoot.ps1" -Html "<某界面>.html" -Out "<某界面>.png"

  说明：HTML 会先复制到 %TEMP%\ka_shot（纯 ASCII 路径）再截图，规避中文路径问题；
        因此 HTML 必须是单文件自包含，内部不要引用相对路径的外部资源。
#>
param(
    [Parameter(Mandatory = $true)][string]$Html,
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$Width = 1440,
    [int]$Height = 900
)

function Find-Browser {
    $cands = @(
        "C:\Program Files\Google\Chrome\Application\chrome.exe",
        "C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
        "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        "C:\Program Files\Microsoft\Edge\Application\msedge.exe"
    )
    foreach ($c in $cands) { if (Test-Path -LiteralPath $c) { return $c } }
    throw "browser not found (Chrome / Edge)"
}

$browser  = Find-Browser
$htmlFull = (Resolve-Path -LiteralPath $Html).Path
if ([System.IO.Path]::IsPathRooted($Out)) { $outFull = $Out } else { $outFull = Join-Path (Get-Location).Path $Out }
$outDir = Split-Path -Parent $outFull
if (-not (Test-Path -LiteralPath $outDir)) { New-Item -ItemType Directory -Force -Path $outDir | Out-Null }

$work = Join-Path $env:TEMP "ka_shot"
New-Item -ItemType Directory -Force -Path $work | Out-Null
$workHtml = Join-Path $work "screen.html"
$workPng  = Join-Path $work "screen.png"
Copy-Item -LiteralPath $htmlFull -Destination $workHtml -Force
if (Test-Path -LiteralPath $workPng) { Remove-Item -LiteralPath $workPng -Force }

$chromeArgs = @(
    "--headless",
    "--disable-gpu",
    "--hide-scrollbars",
    "--force-device-scale-factor=1",
    "--no-first-run",
    "--no-default-browser-check",
    "--disable-extensions",
    "--user-data-dir=$work\profile",
    "--window-size=$Width,$Height",
    "--virtual-time-budget=2500",
    "--screenshot=$workPng",
    ("file:///" + ($workHtml -replace '\\', '/'))
)

$p = Start-Process -FilePath $browser -ArgumentList $chromeArgs -Wait -NoNewWindow -PassThru

# Chrome 偶尔在文件落盘前返回，这里轮询等待
$deadline = (Get-Date).AddSeconds(25)
while ((Get-Date) -lt $deadline) {
    if ((Test-Path -LiteralPath $workPng) -and ((Get-Item -LiteralPath $workPng).Length -gt 1000)) { break }
    Start-Sleep -Milliseconds 200
}
if (-not (Test-Path -LiteralPath $workPng)) { throw ("screenshot failed: " + $htmlFull) }

Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($workPng)
$w = $img.Width
$h = $img.Height
$img.Dispose()

Copy-Item -LiteralPath $workPng -Destination $outFull -Force
$size = (Get-Item -LiteralPath $outFull).Length

if ($w -ne $Width -or $h -ne $Height) {
    Write-Warning ("SIZE MISMATCH: expected {0}x{1}, got {2}x{3} -- check html/body overflow" -f $Width, $Height, $w, $h)
} else {
    Write-Host ("OK  {0}x{1}  {2:N0} bytes  -> {3}" -f $w, $h, $size, $outFull)
}
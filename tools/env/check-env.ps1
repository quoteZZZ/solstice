<#
.SYNOPSIS
  Solstice 开发环境只读体检。不做任何修改，可反复运行。

.DESCRIPTION
  检测：winget / Git / Python / GPU 显存 / 磁盘剩余 / Epic Games Launcher / 虚幻引擎 / VS2022 生成工具(C++)。
  输出「通过 / 缺失」表格与缺失项的可操作补救命令。普通用户权限即可。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\env\check-env.ps1
#>
[CmdletBinding()]
param(
    # 引擎候选根目录不在这里重复声明：find-ue.ps1 是唯一真源（dot-source 后即生效），
    # 需要覆盖时设环境变量 SOLSTICE_UE_SEARCH_ROOTS 或给 find-ue.ps1 传 -SearchRoots。
    # ComfyUI 根目录同理：环境变量 SOLSTICE_COMFYUI_ROOT 优先，其次本参数的默认值。
    [string]$ComfyUIRoot = $(if ($env:SOLSTICE_COMFYUI_ROOT) { $env:SOLSTICE_COMFYUI_ROOT } else { 'D:\ComfyUI' }),
    [int]$DiskWarnGB = 120   # UE + VS + 工程缓存 ≈ 100–120GB
)

$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot '..\lib\Solstice.Common.ps1')

$script:Results = @()
function Add-Result {
    param([string]$Item, [bool]$Ok, [string]$Detail = '', [string]$Fix = '')
    $script:Results += [pscustomobject]@{
        Item   = $Item
        Status = $(if ($Ok) { 'PASS' } else { 'MISSING' })
        Detail = $Detail
        Fix    = $Fix
    }
}

function Get-CmdPath {
    param([string]$Name)
    $c = Get-Command $Name -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    return $null
}

Write-Host '==== Solstice 开发环境体检 ====' -ForegroundColor Cyan
Write-Host ''

# ---- winget ----
$winget = Get-CmdPath 'winget'
if (-not $winget) {
    $guess = Join-Path $env:LOCALAPPDATA 'Microsoft\WindowsApps\winget.exe'
    if (Test-Path -LiteralPath $guess) { $winget = $guess }
}
if ($winget) {
    $wv = (& $winget --version) 2>$null
    Add-Result 'winget' $true ("版本 {0}（{1}）" -f $wv, $winget)
} else {
    Add-Result 'winget' $false '未找到 winget' '从 Microsoft Store 安装“应用安装程序”'
}

# ---- Git ----
$git = Get-CmdPath 'git'
if ($git) {
    Add-Result 'Git' $true ((& git --version) 2>$null)
} else {
    Add-Result 'Git' $false '未找到 git' 'winget install --id Git.Git -e'
}

# ---- Python ----
$py = Get-CmdPath 'python'
if ($py) {
    Add-Result 'Python' $true ("{0}（{1}）" -f ((& python --version) 2>&1), $py)
} else {
    Add-Result 'Python' $false '未找到 python' 'winget install --id Python.Python.3.12 -e'
}

# ---- GPU ----
$smi = Get-CmdPath 'nvidia-smi'
if ($smi) {
    $line = (& nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv,noheader) 2>$null | Select-Object -First 1
    if ($line) { Add-Result 'GPU（NVIDIA）' $true $line }
    else { Add-Result 'GPU（NVIDIA）' $false 'nvidia-smi 无输出' '更新 NVIDIA 驱动' }
} else {
    $vc = Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($vc) { Add-Result 'GPU' $true $vc.Name } else { Add-Result 'GPU' $false '未检测到显卡' '安装显卡驱动' }
}

# ---- 磁盘 ----
foreach ($dev in @('C:', 'D:')) {
    $ld = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='$dev'" -ErrorAction SilentlyContinue
    if ($ld) {
        $free  = [math]::Round($ld.FreeSpace / 1GB, 1)
        $total = [math]::Round($ld.Size / 1GB, 1)
        Add-Result ("磁盘 {0}" -f $dev) ($free -ge $DiskWarnGB) ("剩余 {0} GB / 共 {1} GB" -f $free, $total) ("UE+VS+工程缓存约需 {0}GB" -f $DiskWarnGB)
    }
}

# ---- ComfyUI（本地生图）----
# 不写死模型文件名与层级：扫到 main.py + 任一扩散模型即视为就绪，换模型/换目录都不用改脚本。
$comfyMain = $null
$comfyModels = @()
if (Test-Path -LiteralPath $ComfyUIRoot) {
    $comfyMain = Get-ChildItem -LiteralPath $ComfyUIRoot -Filter 'main.py' -File -Recurse -Depth 2 -ErrorAction SilentlyContinue |
        Select-Object -First 1
    $diffDir = Get-ChildItem -LiteralPath $ComfyUIRoot -Directory -Recurse -Depth 3 -Filter 'diffusion_models' -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($diffDir) {
        $comfyModels = @(Get-ChildItem -LiteralPath $diffDir.FullName -Filter '*.safetensors' -File -ErrorAction SilentlyContinue)
    }
}
$comfyOk = [bool]($comfyMain -and $comfyModels.Count -gt 0)
Add-Result 'ComfyUI（本地生图）' $comfyOk `
    $(if ($comfyOk) { '{0} 就绪（{1} 个扩散模型）' -f $ComfyUIRoot, $comfyModels.Count } else { '未在 {0} 找到 main.py 或扩散模型' -f $ComfyUIRoot }) `
    '可用环境变量 SOLSTICE_COMFYUI_ROOT 指定根目录；见 docs\ops\ENV.md §3'

# ---- Epic Games Launcher ----
$launcherOk = $false; $launcherDetail = ''
$launcherExe = Join-Path ${env:ProgramFiles(x86)} 'Epic Games\Launcher\Portal\Binaries\Win32\EpicGamesLauncher.exe'
if (Test-Path -LiteralPath $launcherExe) { $launcherOk = $true; $launcherDetail = $launcherExe }
elseif (Test-Path -LiteralPath (Join-Path $env:LOCALAPPDATA 'EpicGamesLauncher')) { $launcherOk = $true; $launcherDetail = (Join-Path $env:LOCALAPPDATA 'EpicGamesLauncher') }
else {
    $reg = Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Epic Games\EpicGamesLauncher' -ErrorAction SilentlyContinue
    if ($reg) { $launcherOk = $true; $launcherDetail = '注册表 HKLM\...\Epic Games\EpicGamesLauncher' }
}
Add-Result 'Epic Games Launcher' $launcherOk $launcherDetail 'tools\env\install-epic-launcher.ps1（需用户主动执行）'

# ---- 虚幻引擎 ----
$engines = @(Find-UnrealEngine -All)
if ($engines.Count -gt 0) {
    Add-Result '虚幻引擎 (UE5)' $true (($engines | ForEach-Object { "{0} (v{1})" -f $_.Path, $_.Version }) -join '; ')
} else {
    Add-Result '虚幻引擎 (UE5)' $false '未找到 UnrealEditor.exe' '装 Epic Launcher → 登录 → 安装 UE 最新稳定版（位置任意，会被自动探测）'
}

# ---- VS2022 生成工具 ----
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsOk = $false; $vsDetail = ''
if (Test-Path -LiteralPath $vswhere) {
    $vsPath = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) 2>$null
    if ($vsPath) { $vsOk = $true; $vsDetail = $vsPath }
    else { $vsDetail = '有 VS 但缺组件 Microsoft.VisualStudio.Component.VC.Tools.x86.x64' }
} else { $vsDetail = '未找到 vswhere.exe（无 VS2022）' }
Add-Result 'VS2022 生成工具 (C++)' $vsOk $vsDetail 'tools\env\install-vs-buildtools.ps1（需用户主动执行）'

# ---- 输出 ----
Write-Host ''
$script:Results | Format-Table Item, Status, Detail -AutoSize
$pass = @($script:Results | Where-Object { $_.Status -eq 'PASS' }).Count
$missing = @($script:Results | Where-Object { $_.Status -ne 'PASS' })
Write-Host ('通过：{0}   缺失：{1}' -f $pass, $missing.Count) -ForegroundColor Cyan

if ($missing.Count -gt 0) {
    Write-Host ''
    Write-Host '需要处理的缺失项：' -ForegroundColor Yellow
    foreach ($m in $missing) {
        Write-Host ('  ✗ {0}：{1}' -f $m.Item, $m.Detail) -ForegroundColor Yellow
        if ($m.Fix) { Write-Host ('      → {0}' -f $m.Fix) -ForegroundColor DarkGray }
    }
} else {
    Write-Host ''
    Write-Host '全部通过。可执行 tools\env\configure-vscode.ps1 与 tools\build\gen-projectfiles.ps1。' -ForegroundColor Green
}

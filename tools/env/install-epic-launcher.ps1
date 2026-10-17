<#
.SYNOPSIS
  通过 winget 安装 Epic Games Launcher。

.DESCRIPTION
  ⚠ 这是“由用户主动发起才执行”的操作 —— 不会被其它脚本自动调用。
     安装完成后必须【人工登录 Epic 账号】，才能在 Launcher 内安装虚幻引擎（无法脚本化）。
  幂等：已安装则跳过。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\install-epic-launcher.ps1 -DryRun
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\install-epic-launcher.ps1
#>
[CmdletBinding()]
param(
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$PackageId = 'EpicGames.EpicGamesLauncher'

function Get-Winget {
    $wg = Get-Command winget -ErrorAction SilentlyContinue
    if ($wg) { return $wg.Source }
    $guess = Join-Path $env:LOCALAPPDATA 'Microsoft\WindowsApps\winget.exe'
    if (Test-Path -LiteralPath $guess) { return $guess }
    return $null
}

$winget = Get-Winget
if (-not $winget) {
    Write-Host '未找到 winget，无法自动安装。' -ForegroundColor Red
    Write-Host '请手动下载 Epic Games Launcher：https://store.epicgames.com/download' -ForegroundColor Yellow
    exit 1
}

$launcherExe = Join-Path ${env:ProgramFiles(x86)} 'Epic Games\Launcher\Portal\Binaries\Win32\EpicGamesLauncher.exe'
if ((Test-Path -LiteralPath $launcherExe) -or (Test-Path -LiteralPath (Join-Path $env:LOCALAPPDATA 'EpicGamesLauncher'))) {
    Write-Host '✓ Epic Games Launcher 似乎已安装，跳过。' -ForegroundColor Green
    exit 0
}

$wgArgs = @('install', '--id', $PackageId, '-e', '--accept-package-agreements', '--accept-source-agreements')
Write-Host '将执行：' -ForegroundColor Cyan
Write-Host ("  {0} {1}" -f $winget, ($wgArgs -join ' ')) -ForegroundColor DarkGray

if ($DryRun) { Write-Host '[DryRun] 未实际安装。' -ForegroundColor Yellow; exit 0 }

Write-Host 'ⓘ 安装后需【人工登录 Epic 账号】，才能安装虚幻引擎。' -ForegroundColor Yellow
& $winget @wgArgs
$code = $LASTEXITCODE
if ($code -eq 0) { Write-Host '✓ 安装完成。下一步：打开并登录 Epic 账号，安装 UE 最新稳定版到 D:\UE\UE_5.x' -ForegroundColor Green }
else { Write-Host ("✗ winget 退出码 {0}。可重试或手动安装：https://store.epicgames.com/download" -f $code) -ForegroundColor Red }
exit $code

<#
.SYNOPSIS
  探测引擎路径并写入 .vscode 配置（替换占位符 __UE_ENGINE_ROOT__）。

.DESCRIPTION
  调用 find-ue.ps1 找到引擎根后，替换 .vscode/{c_cpp_properties,settings,launch}.json 中的
  占位符 __UE_ENGINE_ROOT__（替换为正斜杠形式）。
  降级行为：未找到引擎时保持占位状态并给出提示，不破坏 JSON。
  幂等：可反复运行。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\configure-vscode.ps1
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\configure-vscode.ps1 -EngineRoot D:\UE\UE_5.6
#>
[CmdletBinding()]
param(
    [string]$EngineRoot
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'find-ue.ps1')

$Token = '__UE_ENGINE_ROOT__'
$repoRoot  = Split-Path -Parent $PSScriptRoot
$vscodeDir = Join-Path $repoRoot '.vscode'
$targets   = @('c_cpp_properties.json', 'settings.json', 'launch.json')

if (-not (Test-Path -LiteralPath $vscodeDir)) {
    Write-Host ("未找到 .vscode 目录：{0}" -f $vscodeDir) -ForegroundColor Red
    exit 1
}

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $e = Find-UnrealEngine
    if ($e) { $EngineRoot = $e.Path }
}

function Write-Utf8NoBom {
    param([string]$Path, [string]$Text)
    $enc = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $enc)
}

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    Write-Host '⚠ 未找到虚幻引擎，保持占位状态。' -ForegroundColor Yellow
    Write-Host '  1) tools\install-epic-launcher.ps1 → 登录 Epic → 安装 UE 到 D:\UE\UE_5.x' -ForegroundColor Yellow
    Write-Host '  2) 再重跑本脚本。' -ForegroundColor Yellow
    exit 2
}

$forward = ($EngineRoot -replace '\\', '/')
Write-Host ("✓ 引擎根：{0}" -f $EngineRoot) -ForegroundColor Green

foreach ($f in $targets) {
    $p = Join-Path $vscodeDir $f
    if (-not (Test-Path -LiteralPath $p)) { Write-Host ("  - 跳过（不存在）：{0}" -f $f) -ForegroundColor DarkGray; continue }
    # 必须显式按 UTF-8 读取：Windows PowerShell 5.1 的 Get-Content 默认按 ANSI 解析无 BOM 的 UTF-8，
    # 会把文件中的中文注释读成乱码再写回（曾把 .vscode 三份配置写坏）。
    $raw = [System.IO.File]::ReadAllText($p, (New-Object System.Text.UTF8Encoding($false)))
    $new = $raw -replace [regex]::Escape($Token), $forward
    if ($new -ne $raw) {
        Write-Utf8NoBom -Path $p -Text $new
        Write-Host ("  ✓ 已更新 {0}" -f $f) -ForegroundColor Green
    } else {
        Write-Host ("  · {0} 无需更新" -f $f) -ForegroundColor DarkGray
    }
}

Write-Host '完成。请重开 VSCode 或 Reload Window 使 IntelliSense 生效。' -ForegroundColor Cyan
exit 0

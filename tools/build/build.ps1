<#
.SYNOPSIS
  调用引擎自带 Build.bat 编译 Solstice 目标。

.DESCRIPTION
  默认编译 SolsticeEditor Win64 Development。失败时透传退出码。
  注意：改过头文件接口 / 反射宏 / 模块时，须先关闭虚幻编辑器再运行本脚本。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\build.ps1
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Config Shipping -Clean
#>
[CmdletBinding()]
param(
    [string]$Target = 'SolsticeEditor',
    [ValidateSet('Debug', 'DebugGame', 'Development', 'Shipping', 'Test')]
    [string]$Config = 'Development',
    [string]$Platform = 'Win64',
    [string]$EngineRoot,
    [string]$Project,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'find-ue.ps1')

$repoRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $e = Find-UnrealEngine
    if ($e) { $EngineRoot = $e.Path }
}
if ([string]::IsNullOrWhiteSpace($EngineRoot) -or -not (Test-Path -LiteralPath $EngineRoot)) {
    Write-Host '✗ 未找到虚幻引擎，无法编译。' -ForegroundColor Red
    exit 1
}

if ([string]::IsNullOrWhiteSpace($Project)) {
    $cands = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'game') -Filter '*.uproject' -File -ErrorAction SilentlyContinue)
    if ($cands.Count -gt 0) { $Project = $cands[0].FullName }
}
if ([string]::IsNullOrWhiteSpace($Project) -or -not (Test-Path -LiteralPath $Project)) {
    Write-Host '✗ 未找到 .uproject（期望 game\Solstice.uproject）。' -ForegroundColor Red
    exit 1
}

$buildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
if (-not (Test-Path -LiteralPath $buildBat)) {
    Write-Host ("✗ 未找到 Build.bat：{0}" -f $buildBat) -ForegroundColor Red
    exit 1
}

$batArgs = @($Target, $Platform, $Config, "-Project=`"$Project`"", '-WaitMutex')
if ($Clean) { $batArgs += '-Clean' }

Write-Host ("编译：{0} {1} {2} {3}" -f $Target, $Platform, $Config, $(if ($Clean) { '(Clean)' } else { '' })) -ForegroundColor Green
& $buildBat @batArgs
$code = $LASTEXITCODE
if ($code -eq 0) { Write-Host '✓ 编译成功。' -ForegroundColor Green } else { Write-Host ("✗ 编译失败，退出码 {0}。" -f $code) -ForegroundColor Red }
exit $code

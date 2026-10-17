<#
.SYNOPSIS
  启动《至日 · 岁局》的虚幻编辑器。

.DESCRIPTION
  调用 Engine\Binaries\Win64\UnrealEditor.exe 打开 game\Solstice.uproject。
  未编译时 UE 会提示是否自动编译。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\launch-editor.ps1
#>
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [string]$Project,
    [string[]]$ExtraArgs = @('-log')
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'find-ue.ps1')

$repoRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $e = Find-UnrealEngine
    if ($e) { $EngineRoot = $e.Path }
}
if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    Write-Host '✗ 未找到虚幻引擎，无法启动编辑器。' -ForegroundColor Red
    exit 1
}

if ([string]::IsNullOrWhiteSpace($Project)) {
    $cands = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'game') -Filter '*.uproject' -File -ErrorAction SilentlyContinue)
    if ($cands.Count -gt 0) { $Project = $cands[0].FullName }
}
if ([string]::IsNullOrWhiteSpace($Project)) {
    Write-Host '✗ 未找到 .uproject。' -ForegroundColor Red
    exit 1
}

$exe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    Write-Host ("✗ 未找到 UnrealEditor.exe：{0}" -f $exe) -ForegroundColor Red
    exit 1
}

Write-Host ("启动编辑器：{0} `"{1}`"" -f $exe, $Project) -ForegroundColor Green
Start-Process -FilePath $exe -ArgumentList (@("`"$Project`"") + $ExtraArgs) -WorkingDirectory (Split-Path -Parent $Project)
exit 0

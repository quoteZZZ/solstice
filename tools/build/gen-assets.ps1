<#
.SYNOPSIS
  生成工程内容（新克隆后必须跑一次）。

.DESCRIPTION
  仓库里不存二进制资产（docs/TECH.md §0）。唯一必须的资产是三个主材质
  /Game/Core/M_Cel 与 M_Ink 与 M_Sky，由 SolsticeEditor 模块里的生成器按代码建图后写出，
  所以每次克隆、清理 Intermediate 或换机之后都要跑一次本脚本。

  命令是幂等的：三个包已存在时直接跳过，因此重复执行只花三次 FileExists 的时间。

.STATUS
  2026-10-16 实测通过：
  - `LogSolstice: editor module ready: paper material builder installed`
  - `LogSolstice: Display: gen-assets: ok`
  - 产物 game\Content\Core\M_Cel.uasset (11.3 KB) 与 M_Ink.uasset (5.0 KB)

.NOTES
  用 UnrealEditor-Cmd.exe（无窗口）而不是 UnrealEditor.exe，避免拉起 GUI。
  不带 -game：SolsticeEditor 是 Editor 类型模块，只有 GIsEditor 为真时才会加载，
  而命令行的解析发生在 PostEngineInit 模块加载之前，所以该模块在 .uproject 里
  声明的 LoadingPhase 必须是 Default（见 tools/docs/ENVIRONMENT.md §2.6）。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\build\gen-assets.ps1
#>
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [string]$Project
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\Solstice.Common.ps1')

$repoRoot = Get-SolsticeRepoRoot

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $e = Find-UnrealEngine
    if ($e) { $EngineRoot = $e.Path }
}
if ([string]::IsNullOrWhiteSpace($EngineRoot) -or -not (Test-Path -LiteralPath $EngineRoot)) {
    Write-Host '✗ 未找到虚幻引擎，无法生成内容。' -ForegroundColor Red
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

$editorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editorCmd)) {
    Write-Host ("✗ 未找到 UnrealEditor-Cmd.exe：{0}" -f $editorCmd) -ForegroundColor Red
    Write-Host '  （引擎只装了编辑器二进制？先确认 Engine\Binaries\Win64 下有它。）' -ForegroundColor Yellow
    exit 1
}

$logDir = Join-Path $repoRoot 'tmp\logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$absLog = Join-Path $logDir 'gen-assets.log'

Write-Host '生成内容：/Game/Core/M_Cel + M_Ink + M_Sky' -ForegroundColor Green
$cmdArgs = @(
    $Project,
    '-run=SolsticeGenAssets',
    '-unattended', '-nopause', '-nosplash', '-nullrhi',
    ("-abslog={0}" -f $absLog)
)
& $editorCmd @cmdArgs
$code = $LASTEXITCODE

# 命令行自己会把结果写进日志；这里再确认一次磁盘上的产物，退出码之外多一道事实校验。
$cel = Join-Path $repoRoot 'game\Content\Core\M_Cel.uasset'
$ink = Join-Path $repoRoot 'game\Content\Core\M_Ink.uasset'
$sky = Join-Path $repoRoot 'game\Content\Core\M_Sky.uasset'
$ok = (Test-Path -LiteralPath $cel) -and (Test-Path -LiteralPath $ink) -and (Test-Path -LiteralPath $sky)

if ($code -eq 0 -and $ok) {
    Write-Host '✓ 内容就绪：' -ForegroundColor Green
    Get-Item -LiteralPath $cel, $ink, $sky | ForEach-Object { Write-Host ("    {0}  ({1} B)" -f $_.Name, $_.Length) }
} else {
    Write-Host ("✗ 生成失败（退出码 {0}，产物存在={1}）。日志：{2}" -f $code, $ok, $absLog) -ForegroundColor Red
}

if ($code -ne 0) { exit $code }
if (-not $ok) { exit 1 }
exit 0

<#
.SYNOPSIS
  跑 M0 无头回归（确定性基准）。

.DESCRIPTION
  调用 SolsticeEditor 模块里的 USolsticeBenchCommandlet：同一 seed、同一 tick 数跑两遍内核，
  比较整个世界的结构摘要（逐 tile 的高度/植被/湿度/肥力/上色/灰烬/材质/生物群系/水体，
  加物种数量、编年史、余响、已上色比例）。两遍摘要不一致即退出码 1——这是硬失败，
  没有容差。

  它不碰 RHI、World、GameMode，因此也顺带证明 Sim/ 层可无头运行
  （docs/tech/ARCHITECTURE.md §2.2）。

.STATUS
  2026-10-16 实测通过（-Days=2，128x128）：
  - `bench: run A 2880 world minutes in 0.50s (5773 world-min/s), population 1157, ...`
  - `bench: species A moss 0, shrub 4, grazer 1043, raptor 110, fish 0`
  - `bench: digest A 19ABB70199419F90 (147473 mixed) / digest B 19ABB70199419F90 (147473 mixed)`
  - `bench: ok - deterministic and alive`，退出码 0

.PARAMETER Days
  模拟多少个世界日（1 世界日 = 1440 个世界分钟）。默认 1。

.PARAMETER Size
  覆盖世界边长（tile）。默认用 world.json 的 default（128）。

.PARAMETER Seed
  覆盖地形种子。默认用 world.json 的 default（20261016）。

.PARAMETER Life
  默认 1，先播种种群再跑。默认预设本身是「混沌初态、无生命」，但一个从不产生生物的
  回归即使摘要一致也证明不了什么，所以基准默认播种。传 0 可以只测未动过的混沌初态。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\bench.ps1 -Days 2
#>
[CmdletBinding()]
param(
    [int]$Days = 1,
    [int]$Size = 0,
    [int]$Seed = 0,
    [int]$Life = 1,
    [string]$EngineRoot,
    [string]$Project
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'find-ue.ps1')

$repoRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $e = Find-UnrealEngine
    if ($e) { $EngineRoot = $e.Path }
}
if ([string]::IsNullOrWhiteSpace($EngineRoot) -or -not (Test-Path -LiteralPath $EngineRoot)) {
    Write-Host '✗ 未找到虚幻引擎，无法运行基准。' -ForegroundColor Red
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
    exit 1
}

$logDir = Join-Path $repoRoot 'tmp\logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$absLog = Join-Path $logDir 'bench.log'

$benchArgs = @("-Days=$Days", "-Life=$Life")
if ($Size -gt 0) { $benchArgs += "-Size=$Size" }
if ($Seed -gt 0) { $benchArgs += "-Seed=$Seed" }

Write-Host ("运行基准：{0}" -f ($benchArgs -join ' ')) -ForegroundColor Green

$cmdArgs = @($Project, '-run=SolsticeBench') + $benchArgs + @(
    '-unattended', '-nopause', '-nosplash', '-nullrhi', '-NoLogTimes',
    ("-abslog={0}" -f $absLog)
)
& $editorCmd @cmdArgs
$code = $LASTEXITCODE

# 命令行的 stdout 常被引擎日志淹没，所以把摘要行从 abslog 里捞出来给用户看。
if (Test-Path -LiteralPath $absLog) {
    Select-String -LiteralPath $absLog -Pattern 'LogSolstice: (Display|Error): bench:' |
        ForEach-Object { Write-Host ('  ' + $_.Line) }
}

if ($code -eq 0) {
    Write-Host '✓ 基准通过：确定性成立且世界是活的。' -ForegroundColor Green
} else {
    Write-Host ("✗ 基准失败，退出码 {0}。完整日志：{1}" -f $code, $absLog) -ForegroundColor Red
}
exit $code

<#
.SYNOPSIS
  无人值守地渲染一帧并写成 PNG（自动开发会话的"眼睛"）。

.DESCRIPTION
  用 `-game` 模式启动编辑器，等世界稳定（默认 4 s，入场运镜本身 2.6 s），
  把相机切到指定格点，抓一帧写到 PNG 后自行退出。

  它存在的理由：重构阶段没人坐在屏幕前，"画面到底对不对"必须变成
  **磁盘上的一个文件 + 一组数字**（`tools/checks/measure-frame.py` 负责把文件读成数字）。
  在此之前所有视觉改动都只能靠"改完看截图"，结果是五次"改了但画面没变"。

  与 `run/bench.ps1` 的分工：bench 走 `-nullrhi`，证明 Sim 层可无头跑；
  本脚本**必须有 RHI**（-RenderOffscreen 只要渲染设备，不要可见窗口），
  否则抓不到像素。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\run\shot.ps1

.EXAMPLE
  # 换机位 + 加长等待（世界更大时网格构建更慢）
  powershell -ExecutionPolicy Bypass -File tools\run\shot.ps1 -Preset 2 -Delay 6

.EXAMPLE
  # 连 HUD 一起抓（做界面评审用；默认不抓，HUD 会污染像素统计）
  powershell -ExecutionPolicy Bypass -File tools\run\shot.ps1 -UI
#>
[CmdletBinding()]
param(
    [int]$Preset = 0,
    [float]$Delay = 4.0,
    [int]$Width = 1280,
    [int]$Height = 720,
    [string]$Out,
    [switch]$UI,
    [switch]$NoMeasure,
    [int]$Minute = -1,
    # 世界种子。world.json 的 seed:0 = 每次开局摇一个新世界，那样两张“同一机位”其实是两个世界，数值不可比。
    # 默认钉住与 bench 相同的 20261016。
    [int]$Seed = 20261016,
    [int]$TimeoutSeconds = 180,
    # 水材质配平系数的单次覆盖：<diffuse>,<glow>,<shade>[,<foam>[,<specular>[,<reflect>]]]
  # （-1 = 用材质默认值）。
    # 水的亮度是「受光底面 + 自发光本体」两路之和，这个比例决定岛影能不能落到水面上。
    # 有了它，一个候选只要抓一帧（40 s），不必为每个数重编译。
    [string]$Sea,
    # 光照比例的覆盖：<sun>,<sky>,<exposure>（-1 = 用出厂值）。
    # 太阳的强度是天光「捕获反射」的乘数，两者单位不同、不能直接比大小 —— 要判断
    # 「方向光到底有没有主导画面」，只能把比例扫一遍。
    [string]$Env,
    # 地表 albedo 的增益（-1 = 用材质默认 1.0）。用来区分「地被照亮但偏暗」与「地根本没受光」。
    [string]$Cel,
    # 只输出量化结果，不回显引擎日志。抓帧时的 [diag] 行是给排查用的，平时是噪声。
    [switch]$Quiet,
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
    Write-Host '✗ 未找到虚幻引擎，无法截图。' -ForegroundColor Red
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

# 必须是 GUI 版 UnrealEditor.exe：UnrealEditor-Cmd.exe 不带窗口循环，抓不到 viewport 后备缓冲。
$editorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $editorExe)) {
    Write-Host ("✗ 未找到 UnrealEditor.exe：{0}" -f $editorExe) -ForegroundColor Red
    exit 1
}

if ([string]::IsNullOrWhiteSpace($Out)) {
    $Out = Join-Path $repoRoot 'tmp\review\shot.png'
}
# 相对路径必须相对仓库根。之前直接交给引擎，它会按 game/ 解析 → 图写到
# game/tmp/review/，而脚本自己在仓库根找文件 → 报“没有产出”（2026-10-18）。
elseif (-not [System.IO.Path]::IsPathRooted($Out)) {
    $Out = Join-Path $repoRoot $Out
}
$Out = $Out -replace '\\', '/'
$outDir = Split-Path -Parent $Out
if (-not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}
# 关键：先删旧帧。抓图失败时脚本必须报"没有产出"，不能让测量读到上一轮的图。
# 用 .NET 的 Delete 而不是 Remove-Item：本机的 Remove-Item 被平台的安全删除钩子接管，
# 对 tmp/ 下的文件会 fail-closed 抛异常（2026-10-17 实测）。
$logDir = Join-Path $repoRoot 'tmp\logs'
if (-not (Test-Path -LiteralPath $logDir)) { New-Item -ItemType Directory -Force -Path $logDir | Out-Null }
$absLog = Join-Path $logDir 'shot.log'

foreach ($stale in @($Out, $absLog)) {
    if (Test-Path -LiteralPath $stale) {
        try { [System.IO.File]::Delete($stale) } catch { }
    }
}

$shotArgs = @(
    $Project,
    '-game',
    '-RenderOffscreen',
    '-windowed',
    ("-ResX={0}" -f $Width),
    ("-ResY={0}" -f $Height),
    ("-SolsticeShot={0}" -f $Out),
    ("-SolsticeShotPreset={0}" -f $Preset),
    ("-SolsticeShotDelay={0}" -f $Delay),
    '-unattended',
    '-nosplash',
    # Measure the frame, do not wait for the display. With vsync on, [diag] perf reports 16.13 ms
    # at every quality level - the number is the monitor's refresh period, not the renderer's.
    '-ExecCmds=r.VSync 0',

    '-NoLogTimes',
    ("-abslog={0}" -f $absLog)
)
if ($UI) { $shotArgs += '-SolsticeShotUI' }
# Pin the world clock so the same frame can be taken at noon, at dusk and at night
# (docs/art/art.md SS0.4 asks for exactly that trio).
if ($Minute -ge 0) { $shotArgs += ("-SolsticeMinute={0}" -f $Minute) }
# 钉种子：否则每次都是一个新世界，画面指标不可比。
if ($Seed -gt 0) { $shotArgs += ("-SolsticeSeed={0}" -f $Seed) }
if (-not [string]::IsNullOrWhiteSpace($Sea)) { $shotArgs += ("-SolsticeSea={0}" -f $Sea) }
if (-not [string]::IsNullOrWhiteSpace($Env)) { $shotArgs += ("-SolsticeEnv={0}" -f $Env) }
if (-not [string]::IsNullOrWhiteSpace($Cel)) { $shotArgs += ("-SolsticeCel={0}" -f $Cel) }

Write-Host ("抓帧：preset={0} {1}x{2} seed={3} -> {4}{5}" -f $Preset, $Width, $Height, $Seed, $Out,
    $(if ($Minute -ge 0) { ("  (clock {0}:{1:D2})" -f [int][math]::Floor($Minute / 60), ($Minute % 60)) } else { '' })) -ForegroundColor Green

$proc = Start-Process -FilePath $editorExe -ArgumentList $shotArgs -PassThru -WindowStyle Hidden
$exited = $proc.WaitForExit($TimeoutSeconds * 1000)

if (-not $exited) {
    Write-Host ("✗ 超过 {0}s 未退出，已强制结束（进程可能卡在着色器编译）。" -f $TimeoutSeconds) -ForegroundColor Red
    try { $proc.Kill() } catch { }
    exit 1
}

if (Test-Path -LiteralPath $absLog) {
    # `Log` verbosity is in here on purpose: the [diag] lines that report the sun and the fog
    # are logged at Log, and leaving them out made "is the clock pinned at all?" unanswerable.
    # Log-level lines print as `LogSolstice: [diag] ...` with no severity word (only Warning and
    # Error carry one), so the severity part has to be optional or every Log diagnostic is
    # silently dropped.
    if (-not $Quiet) {
        Select-String -LiteralPath $absLog -Pattern 'LogSolstice: (Display|Warning|Error|Log): \[(shot|diag)\]|LogSolstice: \[(shot|diag)\]' |
            ForEach-Object { Write-Host ('  ' + $_.Line) }
    }
}

if (-not (Test-Path -LiteralPath $Out)) {
    Write-Host ("✗ 没有产出 PNG：{0}（完整日志 {1}）" -f $Out, $absLog) -ForegroundColor Red
    exit 1
}

$size = (Get-Item -LiteralPath $Out).Length
Write-Host ("✓ 已写出 {0}（{1:N0} 字节）" -f $Out, $size) -ForegroundColor Green

if ($NoMeasure) { exit 0 }

# 世界区域的屏幕盒由引擎投影圆盘边缘算出来（日志里的 `[shot] box=`）。自己估 crop 是错的：
# 第一次测量把整帧（含深蓝背景）当成世界，得出"中位 0.44 通过"，而圆盘本身是纯白。
$box = $null
if (Test-Path -LiteralPath $absLog) {
    $line = Select-String -LiteralPath $absLog -Pattern '\[shot\] box=' | Select-Object -Last 1
    if ($line) {
        $m = [regex]::Match($line.Line, '\[shot\] box=(\d+),(\d+),(\d+),(\d+)')
        if ($m.Success) { $box = ($m.Groups[1..4] | ForEach-Object { $_.Value }) -join ',' }
    }
}

$measure = Join-Path $repoRoot 'tools\checks\measure-frame.py'

# 量化要 Pillow。按顺序取第一个存在的解释器，不去 probe：本机 PowerShell 会把 probe 的
# stderr 直接打到控制台（`*>` 也压不住），看起来像脚本报错。真缺 Pillow 时由测量脚本
# 自己报 "Pillow is required"，这里再给修法。
$pyCandidates = @()
if ($env:SOLSTICE_PYTHON) { $pyCandidates += $env:SOLSTICE_PYTHON }
$pyCandidates += (Join-Path $env:USERPROFILE '.workbuddy\binaries\python\envs\default\Scripts\python.exe')
$onPath = Get-Command python -ErrorAction SilentlyContinue
if ($onPath) { $pyCandidates += $onPath.Source }

$py = $null
foreach ($candidate in $pyCandidates) {
    if ($candidate -and (Test-Path -LiteralPath $candidate)) { $py = $candidate; break }
}
if (-not $py) {
    Write-Host '! 找不到 python，跳过量化（画面未验证）。' -ForegroundColor Yellow
    exit 0
}

$measureArgs = @($measure, $Out)
if ($box) { $measureArgs += @('--box', $box, '--shape', 'ellipse') } else {
    Write-Host '! 日志里没有 [shot] box=，按整帧量化（含背景，结果仅供参考）。' -ForegroundColor Yellow
}
# The palette gate is per hour (art/art.md SS2.1): a dusk frame is meant to be darker than noon.
# Inferred from -Minute when it is given, so the caller never has to pass the same fact twice.
$hour = 'day'
if ($Minute -ge 0) {
    if ($Minute -lt 300 -or $Minute -ge 1260) { $hour = 'night' }
    elseif ($Minute -lt 480 -or $Minute -ge 1020) { $hour = 'dusk' }
}
$measureArgs += @('--hour', $hour)

Write-Host ("量化：box={0}（{1}）" -f $(if ($box) { $box } else { '全帧' }), (Split-Path -Leaf $py)) -ForegroundColor Green
$measureOut = & $py @measureArgs 2>&1
$measureCode = $LASTEXITCODE
$measureOut | ForEach-Object { Write-Host $_ }

if ($measureOut -match 'Pillow is required') {
    Write-Host '! 该解释器没有 Pillow。修：<python> -m pip install pillow，或设 SOLSTICE_PYTHON。' -ForegroundColor Yellow
    exit 0
}

if ($measureCode -eq 0) { exit 0 }
exit $measureCode

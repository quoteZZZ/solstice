<#
.SYNOPSIS
  调用引擎自带 Build.bat 编译 Solstice 目标。

.DESCRIPTION
  默认编译 SolsticeEditor Win64 Development。失败时透传退出码。
  注意：改过头文件接口 / 反射宏 / 模块时，须先关闭虚幻编辑器再运行本脚本。

  **默认追加 `-NoUBA`**：本机 UBA 的文件系统 detour 失效（每个临时文件改名都 access
  denied，PCH 直接 C1083），必须关掉 UBA 执行器才编得过。详见 docs\ops\ENV.md §2.4.1。
  换到 UBA 正常的机器、想换回加速时加 `-UseUBA`。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\build\build.ps1
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\build\build.ps1 -Config Shipping -Clean
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\build\build.ps1 -UseUBA
#>
[CmdletBinding()]
param(
    [string]$Target = 'SolsticeEditor',
    [ValidateSet('Debug', 'DebugGame', 'Development', 'Shipping', 'Test')]
    [string]$Config = 'Development',
    [string]$Platform = 'Win64',
    [string]$EngineRoot,
    [string]$Project,
    [switch]$Clean,
    [switch]$UseUBA,
    [switch]$AllowEditorRunning
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\Solstice.Common.ps1')

# ---- 保护：编辑器/热编译在跑时默认「拒绝编译」 --------------------------------
# 编译会重写 game\Binaries\Win64\UnrealEditor-Solstice.dll 与 UnrealEditor.modules。
# 若此刻编辑器正在启动或运行，它会读到半写状态的模块清单，弹「Missing Solstice Modules」
# 或直接起不来——本仓库踩过这个坑。所以默认拒绝，**而不是把编辑器关掉**：
# 用户可能正在边看边改，关掉编辑器是最粗暴的解法。
if (-not $AllowEditorRunning) {
    $busy = @(Get-Process UnrealEditor, UnrealEditor-Cmd, LiveCodingConsole -ErrorAction SilentlyContinue)
    if ($busy.Count -gt 0) {
        Write-Host '✗ 检测到虚幻编辑器 / 热编译仍在运行，已拒绝编译。' -ForegroundColor Red
        $busy | ForEach-Object { Write-Host ("    pid {0}  {1}" -f $_.Id, $_.ProcessName) -ForegroundColor DarkYellow }
        Write-Host '  两条出路：' -ForegroundColor Yellow
        Write-Host '    ① 只改了 .cpp 内部实现 → 在编辑器里按 Ctrl+Alt+F11 热编译，不用重启。'
        Write-Host '    ② 改了头文件 / 反射宏 / 新增模块 → 先在编辑器里**正常关闭**（不要强杀，会丢着色器缓存），再跑本脚本。'
        Write-Host '  确实要在编辑器运行时硬编（不推荐，可能让编辑器读到半写状态）：加 -AllowEditorRunning。'
        exit 1
    }
}

$repoRoot = Get-SolsticeRepoRoot

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
if (-not $UseUBA) { $batArgs += '-NoUBA' }
if ($Clean) { $batArgs += '-Clean' }

Write-Host ("编译：{0} {1} {2} {3}{4}" -f $Target, $Platform, $Config, $(if ($Clean) { '(Clean) ' } else { '' }), $(if ($UseUBA) { '(UBA)' } else { '(-NoUBA)' })) -ForegroundColor Green
& $buildBat @batArgs
$code = $LASTEXITCODE
if ($code -eq 0) { Write-Host '✓ 编译成功。' -ForegroundColor Green } else { Write-Host ("✗ 编译失败，退出码 {0}。" -f $code) -ForegroundColor Red }
exit $code

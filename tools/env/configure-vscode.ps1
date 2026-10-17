<#
.SYNOPSIS
  探测虚幻引擎根目录，写入用户级环境变量 SOLSTICE_UE_ENGINE_ROOT（供 .vscode 配置引用）。

.DESCRIPTION
  本脚本**不再改写 .vscode 下的任何文件**。

  历史做法是把占位符 __UE_ENGINE_ROOT__ 替换成本机绝对路径，副作用有三：
    1) 每台机器都要重跑一次，否则 IntelliSense 失效；
    2) 跑完 git 就一直挂着修改；
    3) 一旦误提交，本机路径被推给其它主机，而占位符已被替换掉、脚本也修不回来。
  现在的做法：.vscode/{c_cpp_properties,settings,launch}.json 统一引用 ${env:SOLSTICE_UE_ENGINE_ROOT}，
  本脚本只负责把探测到的引擎根写进**用户级**环境变量（正斜杠、无尾随斜杠）。
  于是这三个文件与主机无关，可安全入库，换机 / 换盘一次搞定。

  优先级：显式 -EngineRoot > 已有环境变量（有效时）> 自动探测（find-ue.ps1）。
  降级行为：未找到引擎时不写环境变量，提示后以退出码 2 结束。
  幂等：可反复运行；值未变化时不重复写入。

.PARAMETER EngineRoot
  显式指定引擎根（含 Engine 子目录的那一层）。默认自动探测。

.PARAMETER DryRun
  只体检：打印将要写入的值与 .vscode 配置的引用情况，不做任何修改。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\env\configure-vscode.ps1
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\env\configure-vscode.ps1 -EngineRoot "D:\Epic Games\UE_5.8"
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\env\configure-vscode.ps1 -DryRun
#>
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\Solstice.Common.ps1')

$VarName   = 'SOLSTICE_UE_ENGINE_ROOT'
$repoRoot = Get-SolsticeRepoRoot
$vscodeDir = Join-Path $repoRoot '.vscode'
$targets   = @('c_cpp_properties.json', 'settings.json', 'launch.json')

if (-not (Test-Path -LiteralPath $vscodeDir)) {
    Write-Host ("✗ 未找到 .vscode 目录：{0}" -f $vscodeDir) -ForegroundColor Red
    exit 1
}

# 引擎根统一用正斜杠、无尾随斜杠：VS Code 的 ${env:...} 拼接后即 D:/Epic Games/UE_5.8/Engine/...
function ConvertTo-ForwardSlash {
    param([Parameter(Mandatory)][string]$Path)
    return (($Path.TrimEnd('\', '/')) -replace '\\', '/')
}

# ============================ 1) 定引擎根 ============================

# 刚写入的用户级变量不一定能被新进程继承（父进程的环境块是旧的），所以两个作用域都读一份，
# 用于「已钉死 / 自动探测」的来源判定与后面的写入比较。
$curUser = [Environment]::GetEnvironmentVariable($VarName, 'User')
$curProc = $env:SOLSTICE_UE_ENGINE_ROOT

$resolved = $null
$source   = ''

if (-not [string]::IsNullOrWhiteSpace($EngineRoot)) {
    if (-not (Test-UeRoot $EngineRoot)) {
        Write-Host ("✗ -EngineRoot 不是有效的引擎根（缺少 Engine\Binaries\Win64\UnrealEditor.exe）：{0}" -f $EngineRoot) -ForegroundColor Red
        exit 1
    }
    $resolved = (Get-Item -LiteralPath $EngineRoot).FullName
    $source   = '显式参数 -EngineRoot'
}

if (-not $resolved) {
    $e = Find-UnrealEngine
    if ($e) {
        $resolved = $e.Path
        $pinned = if ($curProc) { $curProc } else { $curUser }
        $source = if ($pinned -and ((ConvertTo-ForwardSlash -Path $e.Path) -eq (ConvertTo-ForwardSlash -Path $pinned))) { '环境变量（已钉死）' } else { '自动探测' }
    }
}

if (-not $resolved) {
    Write-Host '⚠ 未找到虚幻引擎，环境变量未写入（.vscode 配置保持不变，属预期降级）。' -ForegroundColor Yellow
    Write-Host '  1) tools\env\install-epic-launcher.ps1 → 登录 Epic → 安装 UE 到任意位置' -ForegroundColor Yellow
    Write-Host '  2) 再重跑本脚本' -ForegroundColor Yellow
    exit 2
}

$forward = ConvertTo-ForwardSlash -Path $resolved
Write-Host ("✓ 引擎根：{0}   [来源：{1}]" -f $resolved, $source) -ForegroundColor Green
Write-Host ("  {0} = {1}" -f $VarName, $forward)

if ($curUser) { Write-Host ("  当前用户级变量：{0}" -f $curUser) -ForegroundColor DarkGray }
else { Write-Host '  当前用户级变量：（未设置）' -ForegroundColor DarkGray }

# ============================ 2) 体检 .vscode 的引用 ============================

$refCount = 0
$badRefs  = New-Object System.Collections.Generic.List[string]
$envForm  = '${env:' + $VarName + '}'

foreach ($f in $targets) {
    $p = Join-Path $vscodeDir $f
    if (-not (Test-Path -LiteralPath $p)) { continue }
    $raw = [System.IO.File]::ReadAllText($p, (New-Object System.Text.UTF8Encoding($false)))
    $refCount += ([regex]::Matches($raw, [regex]::Escape($envForm))).Count

    # 找残留的绝对路径（跳过整行注释）：形如 X:/.../Engine/ 或 X:\...\Engine\
    $ln = 0
    foreach ($line in ($raw -split "`r?`n")) {
        $ln++
        if ($line.TrimStart().StartsWith('//')) { continue }
        if ($line -match '[A-Za-z]:[\\/].{0,200}?/Engine/') { $badRefs.Add(("{0}:{1}" -f $f, $ln)) }
    }
}

Write-Host ("  引用 {0} 共 {1} 处" -f $envForm, $refCount) -ForegroundColor DarkGray

# ============================ 3) 写入环境变量 ============================

if ($DryRun) {
    Write-Host '--- DryRun：未做任何修改 ---' -ForegroundColor Cyan
} else {
    if ($curUser -eq $forward) {
        Write-Host '  · 用户级环境变量已是最新，无需写入' -ForegroundColor DarkGray
    } else {
        [Environment]::SetEnvironmentVariable($VarName, $forward, 'User')
        $back = [Environment]::GetEnvironmentVariable($VarName, 'User')
        if ($back -ne $forward) {
            Write-Host '✗ 用户级环境变量写入后回读不一致，请检查权限或组策略。' -ForegroundColor Red
            exit 1
        }
        $old = if ($curUser) { $curUser } else { '（未设置）' }
        Write-Host ("  ✓ 已写入用户级环境变量（原值：{0}）" -f $old) -ForegroundColor Green
    }
    # 让当前进程内后续步骤也立刻可见，无需新开 shell
    [Environment]::SetEnvironmentVariable($VarName, $forward, 'Process')
}

# ============================ 4) 结论 ============================

if ($badRefs.Count -gt 0) {
    Write-Host '' -ForegroundColor Yellow
    Write-Host '⚠ 以下 .vscode 配置里仍有绝对引擎路径（应为 ${env:...} 引用，否则换主机即失效）：' -ForegroundColor Yellow
    foreach ($b in $badRefs) { Write-Host ("    {0}" -f $b) -ForegroundColor Yellow }
    Write-Host '  请改为 ${env:SOLSTICE_UE_ENGINE_ROOT}/Engine/... 后提交。' -ForegroundColor Yellow
}

if ($refCount -eq 0) {
    Write-Host '' -ForegroundColor Yellow
    Write-Host '⚠ .vscode 里没有找到任何 ${env:SOLSTICE_UE_ENGINE_ROOT} 引用，请检查配置是否被改回绝对路径。' -ForegroundColor Yellow
}

Write-Host ''
Write-Host '完成。请**重启 VS Code**（或整进程退出后重开）——环境变量只在进程启动时读取，Reload Window 不够。' -ForegroundColor Cyan
exit 0

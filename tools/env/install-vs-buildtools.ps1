<#
.SYNOPSIS
  通过 winget 安装 Visual Studio 2022 生成工具 + C++ 桌面开发工作负载。

.DESCRIPTION
  ⚠ 这是“由用户主动发起才执行”的操作 —— 不会被其它脚本自动调用。
  体积约 15–20GB，耗时约 20–60 分钟（会拉取 Windows SDK）。
  幂等：已检测到 VC.Tools 组件则跳过（-Force 可强跑以修复/补装）。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\install-vs-buildtools.ps1 -DryRun
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\install-vs-buildtools.ps1
#>
[CmdletBinding()]
param(
    [switch]$DryRun,
    [switch]$Force,
    [switch]$WithClang
)

$ErrorActionPreference = 'Stop'
$PackageId = 'Microsoft.VisualStudio.2022.BuildTools'

$workloads = @(
    'Microsoft.VisualStudio.Workload.VCTools',              # 使用 C++ 的桌面开发
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',    # MSVC v143
    'Microsoft.VisualStudio.Component.Windows11SDK',        # Windows SDK
    'Microsoft.VisualStudio.Component.VC.ATL'               # ATL（UE 部分依赖）
)
if ($WithClang) {
    $workloads += 'Microsoft.VisualStudio.Component.VC.Llvm.Clang'
    $workloads += 'Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset'
}

function Get-Winget {
    $wg = Get-Command winget -ErrorAction SilentlyContinue
    if ($wg) { return $wg.Source }
    $guess = Join-Path $env:LOCALAPPDATA 'Microsoft\WindowsApps\winget.exe'
    if (Test-Path -LiteralPath $guess) { return $guess }
    return $null
}

$winget = Get-Winget
if (-not $winget) {
    Write-Host '未找到 winget，无法自动安装。请手动下载 VS 2022 生成工具：' -ForegroundColor Red
    Write-Host '  https://visualstudio.microsoft.com/zh-hans/downloads/' -ForegroundColor Yellow
    exit 1
}

if (-not $Force) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $p = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) 2>$null
        if ($p) {
            Write-Host ("✓ 已检测到 VS2022 C++ 生成工具：{0}" -f $p) -ForegroundColor Green
            Write-Host '  如需补装组件请加 -Force。' -ForegroundColor DarkGray
            exit 0
        }
    }
}

$override = '--quiet --wait --norestart --nocache'
foreach ($w in $workloads) { $override += (' --add ' + $w) }

$wgArgs = @(
    'install', '--id', $PackageId, '-e',
    '--accept-package-agreements', '--accept-source-agreements',
    '--override', $override
)

Write-Host '将执行：' -ForegroundColor Cyan
Write-Host ("  {0} {1}" -f $winget, ($wgArgs -join ' ')) -ForegroundColor DarkGray
Write-Host '体积约 15–20GB，耗时 20–60 分钟，请耐心等待。' -ForegroundColor Yellow

if ($DryRun) { Write-Host '[DryRun] 未实际安装。' -ForegroundColor Yellow; exit 0 }

& $winget @wgArgs
$code = $LASTEXITCODE
if ($code -eq 0) { Write-Host '✓ 安装完成。可用 tools\check-env.ps1 复检。' -ForegroundColor Green }
else {
    Write-Host ("✗ winget 退出码 {0}。" -f $code) -ForegroundColor Red
    Write-Host '  建议改用 VS 安装器 GUI，勾选「使用 C++ 的桌面开发」，并在“单个组件”中确认 Windows SDK 已选。' -ForegroundColor Yellow
}
exit $code

<#
.SYNOPSIS
  调用 UnrealBuildTool 生成 VSCode 所需的工程文件。

.DESCRIPTION
  -Mode Solution  → 生成 Solstice.sln（供 ms-vscode.cpptools）
  -Mode ClangDb   → 生成 compile_commands.json（供 vscode-clangd）
  优先用引擎自带 dotnet（Engine\Binaries\ThirdParty\DotNet\<ver>\win-x64\dotnet.exe）运行 UnrealBuildTool.dll，
  与引擎 Build.bat 的做法一致；找不到该组合时才回退到 UnrealBuildTool.exe 直调。
  （本机只装 .NET 8/9，而 UBT 需要 .NET 10，直调 .exe 必然失败 —— 见 tools/docs/ENVIRONMENT.md §2.4.1。）
  幂等；失败时透传退出码。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\build\gen-projectfiles.ps1 -Mode Solution
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\build\gen-projectfiles.ps1 -Mode ClangDb
#>
[CmdletBinding()]
param(
    [ValidateSet('Solution', 'ClangDb')]
    [string]$Mode = 'Solution',
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
    Write-Host '✗ 未找到虚幻引擎，无法生成工程文件。先跑 tools\env\find-ue.ps1 排查。' -ForegroundColor Red
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


$ubtDll = Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'

# 引擎自带 dotnet（版本目录倒序，取最高）
$dotnetCandidates = @()
$dotnetRoot = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\DotNet'
if (Test-Path -LiteralPath $dotnetRoot) {
    $dotnetCandidates = @(
        Get-ChildItem -LiteralPath $dotnetRoot -Directory -ErrorAction SilentlyContinue |
            Sort-Object -Property @{ Expression = { try { [version]$_.Name } catch { [version]'0.0' } } } -Descending |
            ForEach-Object { Join-Path $_.FullName 'win-x64\dotnet.exe' }
    )
}
$dotnet = $dotnetCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

$ubtCandidates = @(
    (Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe'),
    (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealBuildTool.exe'),
    (Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool.exe')
)
$ubt = $ubtCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

$useBundledDotnet = [bool]($dotnet -and (Test-Path -LiteralPath $ubtDll))
if (-not $useBundledDotnet -and -not $ubt) {
    Write-Host '✗ 未找到可用的 UnrealBuildTool（既无「引擎自带 dotnet + UnrealBuildTool.dll」，也无 UnrealBuildTool.exe）。' -ForegroundColor Red
    $triedDotnet = if ($dotnetCandidates.Count -gt 0) { $dotnetCandidates -join "`n    " } else { '(未发现 ThirdParty\DotNet 目录)' }
    Write-Host ("  已尝试 dotnet：`n    " + $triedDotnet) -ForegroundColor DarkGray
    Write-Host ("  已尝试 exe   ：`n    " + ($ubtCandidates -join "`n    ")) -ForegroundColor DarkGray
    exit 1
}

Write-Host ("引擎：{0}" -f $EngineRoot) -ForegroundColor Cyan
Write-Host ("工程：{0}" -f $Project) -ForegroundColor Cyan

if ($Mode -eq 'Solution') {
    $ubtArgs = @('-projectfiles', "-project=$Project", '-game', '-rocket', '-progress')
    $label   = '生成 Solstice.sln（cpptools）…'
} else {
    $ubtArgs = @('-Mode=GenerateClangDatabase', "-project=$Project", '-game')
    $label   = '生成 compile_commands.json（clangd）…'
}

if ($useBundledDotnet) {
    Write-Host ("UBT ：{0}" -f $dotnet) -ForegroundColor Cyan
    Write-Host ("      {0}" -f $ubtDll) -ForegroundColor DarkGray
    Write-Host $label -ForegroundColor Green
    # 与引擎 Build.bat 一致：以 Engine\Source 为工作目录运行 UBT
    Push-Location (Join-Path $EngineRoot 'Engine\Source')
    try { & $dotnet $ubtDll @ubtArgs } finally { Pop-Location }
} else {
    Write-Host ("UBT ：{0}" -f $ubt) -ForegroundColor Cyan
    Write-Host '⚠ 未发现引擎自带 dotnet，回退为直调 UnrealBuildTool.exe（要求本机 .NET 运行时 ≥ 10）。' -ForegroundColor Yellow
    Write-Host $label -ForegroundColor Green
    & $ubt @ubtArgs
}
$code = $LASTEXITCODE

$workDir  = Split-Path -Parent $Project
$produced = if ($Mode -eq 'Solution') { Join-Path $workDir 'Solstice.sln' } else { Join-Path $workDir 'compile_commands.json' }

if ($code -eq 0) {
    if (Test-Path -LiteralPath $produced) { Write-Host ("✓ 完成：{0}" -f $produced) -ForegroundColor Green }
    else {
        Write-Host '✓ UBT 退出码 0，但未在预期位置找到产物，请检查日志。' -ForegroundColor Yellow
        Write-Host ("  预期：{0}" -f $produced) -ForegroundColor DarkGray
    }
} else {
    Write-Host ("✗ UBT 失败，退出码 {0}。" -f $code) -ForegroundColor Red
}
exit $code

<#
.SYNOPSIS
  启动 Solstice 的虚幻编辑器。

.DESCRIPTION
  调用 Engine\Binaries\Win64\UnrealEditor.exe 打开 game\Solstice.uproject。
  未编译时 UE 会提示是否自动编译。

  **默认不传 -log**：UE 的 -log 会在 Windows 上额外弹一个日志控制台窗口（标题就是
  exe 路径），看起来像"又开了一个黑框"，而且关掉它等于结束编辑器进程。日志本来就会
  写进 game\Saved\Logs\Solstice.log，所以默认不开控制台；确实想看实时日志时加 -Log。

  加 -Mcp 会额外传入 -ModelContextProtocolStartServer 与 -ModelContextProtocolPort，
  让编辑器进程内嵌的官方 MCP 服务器直接监听。引擎自带的 ModelContextProtocol
  插件默认「不」自动开服（命令行参数与偏好设置两者都关），必须显式开启——原因见
  docs\ops\AI-TOOLING.md §2.2。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\run\launch-editor.ps1

.EXAMPLE
  # 无头（不出渲染窗口）+ 开启 MCP 服务器：http://127.0.0.1:8000/mcp
  powershell -ExecutionPolicy Bypass -File tools\run\launch-editor.ps1 -Mcp -Headless

.NOTES
  经 powershell -File 调用时，-ExtraArgs 只适合单个开关；要传多个用 -Headless
  或直接在 PowerShell 会话里调用：& tools\run\launch-editor.ps1 -Mcp -ExtraArgs @('-NullRHI','-unattended')
#>
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [string]$Project,
    [string[]]$ExtraArgs = @(),
    [switch]$Mcp,
    [uint16]$McpPort = 8000,
    [switch]$Headless,
    [switch]$Log
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\Solstice.Common.ps1')

$repoRoot = Get-SolsticeRepoRoot

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

$launchArgs = @("`"$Project`"") + $ExtraArgs
if ($Log) {
    $launchArgs += @('-log')
}
if ($Headless) {
    $launchArgs += @('-NullRHI', '-unattended', '-nopause')
}
if ($Mcp) {
    $launchArgs += @('-ModelContextProtocolStartServer', "-ModelContextProtocolPort=$McpPort")
}

Write-Host ("启动编辑器：{0} `"{1}`"" -f $exe, $Project) -ForegroundColor Green
if ($Log) {
    Write-Host '  └ -Log 已开：会多一个日志控制台窗口；关掉它等于结束编辑器（日志也在 game\Saved\Logs\Solstice.log）' -ForegroundColor DarkYellow
}
if ($Mcp) {
    Write-Host ("  └ 已开启 MCP 服务器：http://127.0.0.1:{0}/mcp（可用 -McpPort 改端口）" -f $McpPort) -ForegroundColor DarkGray
}
Start-Process -FilePath $exe -ArgumentList $launchArgs -WorkingDirectory (Split-Path -Parent $Project)
exit 0

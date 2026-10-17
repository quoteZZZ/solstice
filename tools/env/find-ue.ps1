<#
.SYNOPSIS
  探测虚幻引擎安装根目录（返回含 Engine 子目录的那一层）。

.DESCRIPTION
  这是**命令行入口**；实现已收敛到 tools\lib\Solstice.Common.ps1：
  候选根推导、Build.version 版本读取、Find-UnrealEngine 都在共享库里，
  本文件只是薄封装，方便命令行直接跑与人工排查。

  其它脚本请 dot-source 共享库，**不要**再 dot-source 本文件。

  只读，不修改任何文件。

.PARAMETER SearchRoots
  候选安装根路径列表。默认从 Epic 官方安装位置与「本机现存盘符 × 常见目录名」推导；
  可用环境变量 SOLSTICE_UE_SEARCH_ROOTS（分号分隔）覆盖。

.PARAMETER All
  列出全部命中（默认只返回版本最高者）。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\env\find-ue.ps1 -All
#>
[CmdletBinding()]
param(
    [string[]]$SearchRoots,
    [switch]$All
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\Solstice.Common.ps1')

$roots = if ($SearchRoots) { $SearchRoots } else { @(Get-DefaultUeSearchRoots) }
$engines = @(Find-UnrealEngine -SearchRoots $roots -All:$All)

if ($engines.Count -eq 0) {
    Write-Host '[find-ue] 未找到虚幻引擎安装。' -ForegroundColor Yellow
    Write-Host '          请先运行 tools\env\install-epic-launcher.ps1，登录 Epic 后安装 UE 最新稳定版；' -ForegroundColor Yellow
    Write-Host '          安装位置任意（默认位置与各盘符常见目录都会被自动探测）；' -ForegroundColor Yellow
    Write-Host '          也可用环境变量 SOLSTICE_UE_SEARCH_ROOTS（分号分隔）指定搜索根。' -ForegroundColor Yellow
    exit 1
}

foreach ($e in $engines) {
    Write-Host ('[find-ue] 引擎 {0}（版本 {1}）' -f $e.Path, $e.Version) -ForegroundColor Green
    Write-Output $e.Path
}
exit 0

<#
.SYNOPSIS
  探测虚幻引擎安装根目录（返回含 Engine 子目录的那一层）。

.DESCRIPTION
  扫描候选位置下的 UE_5* 等目录，通过 Engine\Binaries\Win64\UnrealEditor.exe 确认，
  并读取 Engine\Build\Build.version 得到版本号。
  导出可复用函数 Find-UnrealEngine，供 check-env / configure-vscode / gen-projectfiles / build 等脚本 dot-source。
  只读，不修改任何文件。可被 dot-source（此时只导出函数，不执行 main）。

.PARAMETER SearchRoots
  候选安装根路径列表。

.PARAMETER All
  列出全部命中（默认只返回版本最高者）。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\find-ue.ps1 -All
#>
[CmdletBinding()]
param(
    [string[]]$SearchRoots = @(
        'C:\Program Files\Epic Games',
        'D:\Epic Games',
        'D:\UE',
        'D:\UnrealEngine',
        'C:\UnrealEngine',
        'E:\Epic Games',
        'E:\UE'
    ),
    [switch]$All
)

function Get-UeVersion {
    param([string]$EngineRoot)
    $bv = Join-Path $EngineRoot 'Engine\Build\Build.version'
    if (Test-Path -LiteralPath $bv) {
        try {
            $j = Get-Content -LiteralPath $bv -Raw | ConvertFrom-Json
            return ('{0}.{1}.{2}' -f $j.MajorVersion, $j.MinorVersion, $j.PatchVersion)
        } catch { return 'unknown' }
    }
    return 'unknown'
}

function Test-UeRoot {
    param([string]$Root)
    if ([string]::IsNullOrWhiteSpace($Root)) { return $false }
    if (-not (Test-Path -LiteralPath $Root)) { return $false }
    return (Test-Path -LiteralPath (Join-Path $Root 'Engine\Binaries\Win64\UnrealEditor.exe'))
}

function Find-UnrealEngine {
    <#
      返回 [pscustomobject]@{ Path = 引擎根; Version = 版本号 } 的数组。
    #>
    param(
        [string[]]$Roots = $SearchRoots,
        [switch]$All
    )
    $found = New-Object System.Collections.Generic.List[object]

    foreach ($root in $Roots) {
        if ([string]::IsNullOrWhiteSpace($root)) { continue }
        if (-not (Test-Path -LiteralPath $root)) { continue }

        $candidates = New-Object System.Collections.Generic.List[string]
        if (Test-UeRoot $root) { $candidates.Add((Get-Item -LiteralPath $root).FullName) }

        # 命名优先：UE_5*
        foreach ($d in (Get-ChildItem -LiteralPath $root -Directory -Filter 'UE_5*' -ErrorAction SilentlyContinue)) {
            if (Test-UeRoot $d.FullName) { $candidates.Add($d.FullName) }
        }
        # 兜底：任意一级子目录
        foreach ($d in (Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue)) {
            if (Test-UeRoot $d.FullName) { $candidates.Add($d.FullName) }
        }

        foreach ($c in ($candidates | Select-Object -Unique)) {
            $found.Add([pscustomobject]@{ Path = $c; Version = (Get-UeVersion $c) })
        }
    }

    $unique = $found | Sort-Object Path -Unique
    if ($All) { return $unique }
    return ($unique | Sort-Object Version -Descending | Select-Object -First 1)
}

# 仅在作为脚本直接运行时执行 main
if ($MyInvocation.InvocationName -ne '.') {
    $engines = @(Find-UnrealEngine -Roots $SearchRoots -All:$All)
    if ($engines.Count -eq 0) {
        Write-Host '[find-ue] 未找到虚幻引擎安装。' -ForegroundColor Yellow
        Write-Host '          请先运行 tools\install-epic-launcher.ps1，登录 Epic 后安装 UE 最新稳定版到 D:\UE\UE_5.x' -ForegroundColor Yellow
        exit 1
    }
    foreach ($e in $engines) {
        Write-Host ('[find-ue] 引擎 {0}（版本 {1}）' -f $e.Path, $e.Version) -ForegroundColor Green
        Write-Output $e.Path
    }
    exit 0
}

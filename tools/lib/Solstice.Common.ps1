<#
.SYNOPSIS
  Solstice 工具链共享库：引擎 / 工程 / 可执行文件的**唯一实现**。

.DESCRIPTION
  tools\ 下所有脚本都 dot-source 本文件，不再各写一份定位逻辑。提供：

    Get-SolsticeRepoRoot        仓库根（本文件固定在 tools\lib\，与调用方位置无关）
    Get-DefaultUeSearchRoots    引擎候选根（盘符不写死；SOLSTICE_UE_SEARCH_ROOTS 可覆盖）
    Find-UnrealEngine           扫描候选根，返回 @{Path;Version}（默认取版本最高者）
    Resolve-SolsticeEngineRoot  解析引擎根：显式参数 > SOLSTICE_UE_ENGINE_ROOT > 探测；找不到返回 $null
    Resolve-SolsticeProject     解析 .uproject：显式参数 > 扫 game\*.uproject；找不到返回 $null
    Resolve-SolsticeExe         解析引擎可执行文件（-Kind Editor | Cmd），找不到返回 $null

.USAGE
  $ErrorActionPreference = 'Stop'
  . (Join-Path $PSScriptRoot '..\lib\Solstice.Common.ps1')

.NOTES
  只定义函数、不改任何文件。被 dot-source 时不执行 main。
  保存编码：**UTF-8 with BOM**（PS 5.1 下中文注释/输出需要）。
#>

# 本文件固定在 tools\lib\，所以仓库根 = 本文件的祖父目录。与调用方所在子目录无关。
$script:SolsticeToolsDir = Split-Path -Parent $PSScriptRoot
$script:SolsticeRepoRoot = Split-Path -Parent $script:SolsticeToolsDir

function Get-SolsticeRepoRoot {
    <# 返回仓库根绝对路径（形如 D:\GitCode\solstice）。 #>
    return $script:SolsticeRepoRoot
}

function Get-SolsticeToolsDir {
    <# 返回 tools\ 绝对路径。 #>
    return $script:SolsticeToolsDir
}

function Get-DefaultUeSearchRoots {
    <#
      引擎候选根目录的**唯一真源**。
      盘符不写死：从 Epic 官方安装位置与"本机现存盘符 × 常见目录名"动态推导；
      需要固定时用环境变量 SOLSTICE_UE_SEARCH_ROOTS（分号分隔）或显式参数覆盖。
    #>
    if ($env:SOLSTICE_UE_SEARCH_ROOTS) {
        return @($env:SOLSTICE_UE_SEARCH_ROOTS -split ';' | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    }
    $roots = New-Object System.Collections.Generic.List[string]
    foreach ($pf in @(${env:ProgramFiles}, ${env:ProgramFiles(x86)})) {
        if ($pf) { $roots.Add((Join-Path $pf 'Epic Games')) }
    }
    foreach ($drive in (Get-PSDrive -PSProvider FileSystem -ErrorAction SilentlyContinue)) {
        if (-not $drive.Root) { continue }
        foreach ($name in @('Epic Games', 'UE', 'UnrealEngine')) { $roots.Add((Join-Path $drive.Root $name)) }
    }
    return @($roots | Select-Object -Unique)
}

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
    <# 判断一个目录是否是合法的引擎根（含 Engine\Binaries\Win64\UnrealEditor.exe）。 #>
    param([string]$Root)
    if ([string]::IsNullOrWhiteSpace($Root)) { return $false }
    if (-not (Test-Path -LiteralPath $Root)) { return $false }
    return (Test-Path -LiteralPath (Join-Path $Root 'Engine\Binaries\Win64\UnrealEditor.exe'))
}

function Find-UnrealEngine {
    <#
      返回 [pscustomobject]@{ Path = 引擎根; Version = 版本号 }。
      -All 时返回全部命中（去重），否则只返回版本最高者。

      优先级：SOLSTICE_UE_ENGINE_ROOT（有效时直接返回）> 扫描候选根。
    #>
    param(
        [string[]]$SearchRoots = @(Get-DefaultUeSearchRoots),
        [switch]$All
    )

    # 最高优先级：SOLSTICE_UE_ENGINE_ROOT 显式钉死引擎根。多引擎共存 / 换机 / 换盘时用它，
    # 与 tools/docs/ENVIRONMENT.md §5.1 的「显式参数 > 环境变量 > 自动探测 > 内置默认」一致。
    # .vscode 三个配置里的 ${env:SOLSTICE_UE_ENGINE_ROOT} 就是同一个变量
    # （由 tools\env\configure-vscode.ps1 写入用户级环境变量）。值无效时自动退回探测。
    if (-not $All) {
        # 两个作用域都读：新进程未必继承到刚写入的用户级变量（父进程环境块是旧的）。
        $pinned = $env:SOLSTICE_UE_ENGINE_ROOT
        if (-not $pinned) { $pinned = [Environment]::GetEnvironmentVariable('SOLSTICE_UE_ENGINE_ROOT', 'User') }
        if ($pinned -and (Test-UeRoot $pinned)) {
            return [pscustomobject]@{ Path = (Get-Item -LiteralPath $pinned).FullName; Version = (Get-UeVersion $pinned) }
        }
    }

    $found = New-Object System.Collections.Generic.List[object]

    foreach ($root in $SearchRoots) {
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

function Resolve-SolsticeEngineRoot {
    <#
      解析引擎根：显式 -EngineRoot（须真实存在）优先，其次环境变量 SOLSTICE_UE_ENGINE_ROOT
      （由 Find-UnrealEngine 内部处理），最后自动探测。
      找不到返回 $null —— 由调用方决定报错文案与退出码。
    #>
    param([string]$EngineRoot)
    if (-not [string]::IsNullOrWhiteSpace($EngineRoot)) {
        if (Test-Path -LiteralPath $EngineRoot) { return (Get-Item -LiteralPath $EngineRoot).FullName }
        return $null
    }
    $e = Find-UnrealEngine
    if ($e) { return $e.Path }
    return $null
}

function Resolve-SolsticeProject {
    <#
      解析 .uproject：显式 -Project（须真实存在）优先，否则扫仓库 game\ 下的第一个 *.uproject。
      找不到返回 $null。
    #>
    param([string]$Project)
    if (-not [string]::IsNullOrWhiteSpace($Project)) {
        if (Test-Path -LiteralPath $Project) { return (Get-Item -LiteralPath $Project).FullName }
        return $null
    }
    $gameDir = Join-Path $script:SolsticeRepoRoot 'game'
    $cands = @(Get-ChildItem -LiteralPath $gameDir -Filter '*.uproject' -File -ErrorAction SilentlyContinue)
    if ($cands.Count -gt 0) { return $cands[0].FullName }
    return $null
}

function Resolve-SolsticeExe {
    <#
      解析引擎可执行文件。
        -Kind Editor → Engine\Binaries\Win64\UnrealEditor.exe      （GUI / PIE / -game）
        -Kind Cmd    → Engine\Binaries\Win64\UnrealEditor-Cmd.exe  （无窗口 commandlet）
      找不到返回 $null。
    #>
    param(
        [Parameter(Mandatory = $true)][string]$EngineRoot,
        [ValidateSet('Editor', 'Cmd')][string]$Kind = 'Editor'
    )
    if ([string]::IsNullOrWhiteSpace($EngineRoot)) { return $null }
    $name = if ($Kind -eq 'Cmd') { 'UnrealEditor-Cmd.exe' } else { 'UnrealEditor.exe' }
    $p = Join-Path $EngineRoot ('Engine\Binaries\Win64\' + $name)
    if (Test-Path -LiteralPath $p) { return (Get-Item -LiteralPath $p).FullName }
    return $null
}

function New-SolsticeLogPath {
    <#
      统一的临时日志路径：<仓库根>\tmp\logs\<名称>.log，并确保目录存在。
      tmp\ 已在 .gitignore 中，可作为一次性诊断日志的落点。
    #>
    param([Parameter(Mandatory = $true)][string]$Name)
    $dir = Join-Path $script:SolsticeRepoRoot 'tmp\logs'
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    return (Join-Path $dir ($Name + '.log'))
}

<#
.SYNOPSIS
  CodeBuddy 用户级配置的「仓库模板 <-> 本机」同步（跨主机换机用）。

.DESCRIPTION
  VS Code 内置的设置同步（Settings Sync）只覆盖 VS Code 自己的东西：
    settings / keybindings / snippets / tasks / 扩展清单 / MCP
  CodeBuddy 的用户级配置**不在其中**，它们躺在 %USERPROFILE%\.codebuddy\ 下：
    settings.json   插件集、权限模式、记忆开关、网关凭据
    mcp.json        MCP 服务器列表
  本脚本用仓库模板补上这一段。于是换一台主机的完整动作是两步：

    1) 云端：VS Code 登录并开启 Settings Sync（左下角账户图标 → Turn on Settings Sync…）
       → 用户设置 / 快捷键 / 片段 / 扩展清单自动到位。
    2) 兜底：git clone 本仓库 → 跑本脚本 -Apply
       → CodeBuddy 的插件集、权限模式、记忆开关、MCP 到位。
       再按提示补两处**密钥类**配置（模板里刻意不含）。

  模板里的密钥一律剥离，绝不入库：
    gateway.password、env.CODEBUDDY_API_KEY
  -Apply 之后脚本会明确列出哪些需要人工补、补在哪里。

  模式（默认只读，不改任何东西）：
    无开关                只读对比：逐项列出「本机 / 模板」的差异
    -Export               本机 -> 模板（自动剥离密钥；导出后记得 git diff 复核）
    -Apply                模板 -> 本机（先备份；只覆盖模板里声明的键，本机多出来的键保留）
    -CodeBuddyHome <path> 指定 .codebuddy 目录（默认 %USERPROFILE%\.codebuddy）

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\ai-config\sync-ai-config.ps1
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\ai-config\sync-ai-config.ps1 -Apply
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\ai-config\sync-ai-config.ps1 -Export
#>
[CmdletBinding()]
param(
    [switch]$Export,
    [switch]$Apply,
    [string]$CodeBuddyHome
)

$ErrorActionPreference = 'Stop'

if ($Export -and $Apply) {
    Write-Host '✗ -Export 与 -Apply 不能同时用（一个方向一个开关）。' -ForegroundColor Red
    exit 1
}

$tplDir = $PSScriptRoot
$cbHome = if ($CodeBuddyHome) { $CodeBuddyHome } else { Join-Path $env:USERPROFILE '.codebuddy' }

# 模板文件 -> 本机目标文件
#   StripOnExport = 永不入库的顶层键：密钥（gateway）+ 运行态（codebuddy 的 agentHome/mainAgent）
#   ManualKeys    = 换机后必须人工补的**密钥类**键（会单独提示）
$pairs = @(
    [pscustomobject]@{
        Tpl           = 'codebuddy-settings.json'
        Target        = 'settings.json'
        StripOnExport = @('gateway', 'codebuddy')
        ManualKeys    = @('gateway')
    },
    [pscustomobject]@{
        Tpl           = 'codebuddy-mcp.json'
        Target        = 'mcp.json'
        StripOnExport = @()
        ManualKeys    = @()
    }
)

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Read-JsonFile {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    $raw = [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8)
    if ([string]::IsNullOrWhiteSpace($raw)) { return $null }
    return ($raw | ConvertFrom-Json)
}

# PS 5.1 的 ConvertTo-Json 只有 4 空格缩进、且会把冒号排得歪歪扭扭（`"k":  v`）。
# -Export 会把它写回仓库模板，格式难看会被当成噪声改动，所以自己排一遍：2 空格、冒号后单空格。
function ConvertTo-PrettyJson {
    param($Node, [int]$Indent = 0)
    $pad = ' ' * $Indent
    $pad2 = ' ' * ($Indent + 2)

    if ($null -eq $Node) { return 'null' }
    if ($Node -is [bool]) { return $(if ($Node) { 'true' } else { 'false' }) }

    if ($Node -is [System.Management.Automation.PSCustomObject]) {
        $props = @($Node.PSObject.Properties)
        if ($props.Count -eq 0) { return '{}' }
        $lines = foreach ($p in $props) {
            $pad2 + (ConvertTo-Json -InputObject $p.Name -Compress) + ': ' + (ConvertTo-PrettyJson -Node $p.Value -Indent ($Indent + 2))
        }
        return '{' + "`n" + ($lines -join ",`n") + "`n" + $pad + '}'
    }

    if (($Node -is [System.Collections.IEnumerable]) -and ($Node -isnot [string])) {
        $items = @($Node)
        if ($items.Count -eq 0) { return '[]' }
        $lines = foreach ($item in $items) { $pad2 + (ConvertTo-PrettyJson -Node $item -Indent ($Indent + 2)) }
        return '[' + "`n" + ($lines -join ",`n") + "`n" + $pad + ']'
    }

    return (ConvertTo-Json -InputObject $Node -Compress)
}

function Write-JsonFile {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)]$Node
    )
    $json = ConvertTo-PrettyJson -Node $Node
    [System.IO.File]::WriteAllText($Path, $json + "`r`n", $utf8NoBom)
}

# 递归规范化成「键有序」的 JSON 文本，用于稳定比较（否则键顺序不同会被误判为差异）
function ConvertTo-Canonical {
    param($Node)
    if ($null -eq $Node) { return 'null' }
    if ($Node -is [System.Management.Automation.PSCustomObject]) {
        $parts = foreach ($p in ($Node.PSObject.Properties | Sort-Object Name)) {
            '"' + $p.Name + '":' + (ConvertTo-Canonical -Node $p.Value)
        }
        return '{' + ($parts -join ',') + '}'
    }
    if (($Node -is [System.Collections.IEnumerable]) -and ($Node -isnot [string])) {
        $parts = foreach ($item in $Node) { ConvertTo-Canonical -Node $item }
        return '[' + ($parts -join ',') + ']'
    }
    return (ConvertTo-Json -InputObject $Node -Compress)
}

# 模板覆盖本机：递归合并；本机有而模板没有的键**保留**（不误删未知配置）
function Merge-Json {
    param($Template, $Target)
    if ($Template -isnot [System.Management.Automation.PSCustomObject]) { return $Template }

    $out = [ordered]@{}
    if ($Target -is [System.Management.Automation.PSCustomObject]) {
        foreach ($p in $Target.PSObject.Properties) { $out[$p.Name] = $p.Value }
    }
    foreach ($p in $Template.PSObject.Properties) {
        $old = $null
        if ($Target -is [System.Management.Automation.PSCustomObject]) {
            $hit = $Target.PSObject.Properties[$p.Name]
            if ($hit) { $old = $hit.Value }
        }
        $out[$p.Name] = Merge-Json -Template $p.Value -Target $old
    }
    return [pscustomobject]$out
}

function Remove-SecretKeys {
    param($Node, [string[]]$Keys)
    if ($Node -isnot [System.Management.Automation.PSCustomObject]) { return $Node }
    $out = [ordered]@{}
    foreach ($p in $Node.PSObject.Properties) {
        if ($Keys -contains $p.Name) { continue }
        $out[$p.Name] = $p.Value
    }
    $result = [pscustomobject]$out
    # env 里可能藏着 API 密钥（历史版本会把 CODEBUDDY_API_KEY 写在这里），一并剥掉
    if ($result.PSObject.Properties['env'] -and $result.env -is [System.Management.Automation.PSCustomObject]) {
        if ($result.env.PSObject.Properties['CODEBUDDY_API_KEY']) {
            $envOut = [ordered]@{}
            foreach ($p in $result.env.PSObject.Properties) {
                if ($p.Name -eq 'CODEBUDDY_API_KEY') { continue }
                $envOut[$p.Name] = $p.Value
            }
            $result.env = [pscustomobject]$envOut
        }
    }
    return $result
}

Write-Host ''
Write-Host ('CodeBuddy 配置目录：{0}   [{1}]' -f $cbHome, $(if (Test-Path -LiteralPath $cbHome) { '存在' } else { '不存在' })) -ForegroundColor Cyan
Write-Host ('模板目录：{0}' -f $tplDir) -ForegroundColor DarkGray

$manual = New-Object System.Collections.Generic.List[string]
$changed = 0

foreach ($pair in $pairs) {
    $tplPath = Join-Path $tplDir $pair.Tpl
    $dstPath = Join-Path $cbHome $pair.Target

    Write-Host ''
    Write-Host ('=== {0}  <->  {1} ===' -f $pair.Tpl, $pair.Target) -ForegroundColor White

    if (-not (Test-Path -LiteralPath $tplPath)) {
        Write-Host ('  ✗ 模板缺失：{0}' -f $tplPath) -ForegroundColor Red
        continue
    }

    $tpl = Read-JsonFile -Path $tplPath
    $cur = Read-JsonFile -Path $dstPath

    if ($Export) {
        if (-not $cur) {
            Write-Host ('  ✗ 本机没有 {0}，无法导出。' -f $dstPath) -ForegroundColor Yellow
            continue
        }
        $clean = Remove-SecretKeys -Node $cur -Keys $pair.StripOnExport
        Write-JsonFile -Path $tplPath -Node $clean
        $stripped = if ($pair.StripOnExport.Count -gt 0) { '（已剥离：' + ($pair.StripOnExport -join ', ') + '）' } else { '（无需剥离）' }
        Write-Host ('  ✓ 已导出到模板 {0} {1}' -f $pair.Tpl, $stripped) -ForegroundColor Green
        $changed++
        continue
    }

    if ($Apply) {
        if (-not $cur) {
            Write-Host ('  · 本机不存在，按模板新建：{0}' -f $dstPath) -ForegroundColor DarkGray
        } else {
            $ts = Get-Date -Format 'yyyyMMdd-HHmmss'
            $bak = '{0}.bak-{1}' -f $dstPath, $ts
            Copy-Item -LiteralPath $dstPath -Destination $bak -Force
            Write-Host ('  · 已备份 -> {0}' -f (Split-Path -Leaf $bak)) -ForegroundColor DarkGray
        }
        if (-not (Test-Path -LiteralPath $cbHome)) { New-Item -ItemType Directory -Path $cbHome -Force | Out-Null }
        $merged = Merge-Json -Template $tpl -Target $cur
        Write-JsonFile -Path $dstPath -Node $merged
        Write-Host ('  ✓ 已写入 {0}' -f $dstPath) -ForegroundColor Green
        $changed++
        continue
    }

    # ---- 默认：只读对比 ----
    if (-not $cur) {
        Write-Host ('  · 本机不存在（-Apply 会按模板新建）' -f $dstPath) -ForegroundColor Yellow
        continue
    }

    $names = New-Object System.Collections.Generic.List[string]
    foreach ($p in $tpl.PSObject.Properties) { $names.Add($p.Name) }
    foreach ($p in $cur.PSObject.Properties) { if (-not $names.Contains($p.Name)) { $names.Add($p.Name) } }

    $diffCount = 0
    foreach ($n in ($names | Sort-Object)) {
        $tv = $tpl.PSObject.Properties[$n]
        $cv = $cur.PSObject.Properties[$n]
        $tText = if ($tv) { ConvertTo-Canonical -Node $tv.Value } else { $null }
        $cText = if ($cv) { ConvertTo-Canonical -Node $cv.Value } else { $null }

        if (-not $tv) { Write-Host ('  [仅本机] {0}' -f $n) -ForegroundColor DarkYellow; $diffCount++; continue }
        if (-not $cv) { Write-Host ('  [仅模板] {0}   → -Apply 会补上' -f $n) -ForegroundColor Yellow; $diffCount++; continue }
        if ($tText -eq $cText) { Write-Host ('  [一致]   {0}' -f $n) -ForegroundColor DarkGray; continue }

        Write-Host ('  [不同]   {0}' -f $n) -ForegroundColor Yellow
        Write-Host ('             本机：{0}' -f $cText) -ForegroundColor DarkGray
        Write-Host ('             模板：{0}' -f $tText) -ForegroundColor DarkGray
        $diffCount++
    }

    if ($pair.ManualKeys.Count -gt 0) {
        foreach ($s in $pair.ManualKeys) {
            if ($cur.PSObject.Properties[$s]) { $manual.Add(('{0} 里的 "{1}"（本机有、模板刻意不含）' -f $pair.Target, $s)) }
        }
    }
    if ($diffCount -eq 0) { Write-Host '  ✓ 与模板完全一致' -ForegroundColor Green }
}

# ---- 需要人工补的密钥类配置 ----
$keyVar = [Environment]::GetEnvironmentVariable('CODEBUDDY_API_KEY', 'User')
if (-not $keyVar) { $manual.Add('用户级环境变量 CODEBUDDY_API_KEY（API 密钥，刻意不入库）') }

if ($manual.Count -gt 0) {
    Write-Host ''
    Write-Host '以下**密钥类**配置刻意不进仓库，换机后需要人工补：' -ForegroundColor Yellow
    foreach ($m in $manual) { Write-Host ('  · {0}' -f $m) -ForegroundColor Yellow }
    Write-Host '  补法：CodeBuddy IDE 里登录一次即可自动写入；或参照 docs/ops/AI-TOOLING.md §4。' -ForegroundColor Yellow
}

# ---- VS Code 侧同步状态（只读，便于确认"云端那条路"通不通）----
$storage = Join-Path $env:APPDATA 'Code\User\globalStorage\storage.json'
if (Test-Path -LiteralPath $storage) {
    $syncOn = $false
    try {
        $raw = [System.IO.File]::ReadAllText($storage, [System.Text.Encoding]::UTF8)
        $syncOn = ($raw -match '"userDataSync\.')
    } catch { }
    Write-Host ''
    if ($syncOn) {
        Write-Host 'VS Code 设置同步：已启用（设置 / 快捷键 / 片段 / 扩展会跟随账号）' -ForegroundColor Green
    } else {
        Write-Host 'VS Code 设置同步：**未启用** —— 左下角账户图标 → Turn on Settings Sync… → 登录 GitHub/Microsoft' -ForegroundColor Yellow
    }
}

Write-Host ''
if ($changed -gt 0) { Write-Host ('完成，处理了 {0} 个文件。' -f $changed) -ForegroundColor Cyan }
elseif (-not $Export -and -not $Apply) { Write-Host '只读对比结束（未修改任何文件）。要落地用 -Apply 或 -Export。' -ForegroundColor Cyan }

exit 0

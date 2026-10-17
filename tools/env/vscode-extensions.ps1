<#
.SYNOPSIS
  VSCode 扩展体检与管理（面向 Solstice）。

.DESCRIPTION
  读取 .vscode\extensions.json 的 recommendations / unwantedRecommendations，
  与本机已装扩展对比，输出「已装 / 缺失 / 多余」三类清单。

  默认只做只读体检（不安装、不卸载）。

  要真正增删必须显式给开关：
    -InstallRecommended   安装缺失的推荐扩展
    -RemoveUnwanted       卸载与本项目无关的扩展（默认先预览；确认需再加 -Confirm）

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\env\vscode-extensions.ps1
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\env\vscode-extensions.ps1 -InstallRecommended
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\env\vscode-extensions.ps1 -RemoveUnwanted
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\env\vscode-extensions.ps1 -RemoveUnwanted -Confirm
#>
[CmdletBinding()]
param(
    [switch]$InstallRecommended,
    [switch]$RemoveUnwanted,
    [switch]$Confirm,
    [string]$CodeCli
)

$ErrorActionPreference = 'Stop'

# ============================ 工具函数 ============================

function Resolve-CodeCli {
    param([string]$Explicit)
    if ($Explicit -and (Test-Path -LiteralPath $Explicit)) { return $Explicit }

    $cmd = Get-Command code -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    # 盘符不写死：优先环境变量 SOLSTICE_VSCODE_CLI，其次标准安装位置（由系统变量推导）。
    $candidates = @(
        $env:SOLSTICE_VSCODE_CLI,
        (Join-Path $env:LOCALAPPDATA 'Programs\Microsoft VS Code\bin\code.cmd'),
        (Join-Path ${env:ProgramFiles} 'Microsoft VS Code\bin\code.cmd'),
        (Join-Path $env:LOCALAPPDATA 'Programs\Microsoft VS Code Insiders\bin\code-insiders.cmd')
    )
    foreach ($c in $candidates) { if ($c -and (Test-Path -LiteralPath $c)) { return $c } }
    return $null
}

# 读 JSONC（容忍 // 与 /* */ 注释、尾随逗号）—— VSCode 的配置文件都是 JSONC
function Read-Jsonc {
    param([Parameter(Mandatory)][string]$Path)

    $raw = [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8)
    # 去块注释
    $raw = [regex]::Replace($raw, '/\*.*?\*/', '', [System.Text.RegularExpressions.RegexOptions]::Singleline)
    # 去行注释（本项目这两个 json 的字符串里不含 "//"，可安全处理）
    $raw = [regex]::Replace($raw, '(?m)//.*$', '')
    # 去尾随逗号
    $raw = [regex]::Replace($raw, ',(\s*[}\]])', '$1')
    return ($raw | ConvertFrom-Json)
}

# ============================ 主流程 ============================

. (Join-Path $PSScriptRoot '..\lib\Solstice.Common.ps1')

$repoRoot = Get-SolsticeRepoRoot
$extJson  = Join-Path $repoRoot '.vscode\extensions.json'

if (-not (Test-Path -LiteralPath $extJson)) {
    Write-Host ("✗ 未找到 {0}" -f $extJson) -ForegroundColor Red
    exit 1
}

$code = Resolve-CodeCli -Explicit $CodeCli
if (-not $code) {
    Write-Host '✗ 未找到 VSCode CLI（code.cmd）。请用 -CodeCli 指定完整路径，' -ForegroundColor Red
    Write-Host '  例如：-CodeCli "<你的 code.cmd 完整路径>"；或设环境变量 SOLSTICE_VSCODE_CLI' -ForegroundColor Yellow
    exit 1
}
Write-Host ("VSCode CLI：{0}" -f $code) -ForegroundColor Cyan
Write-Host ("版本      ：{0}" -f ((& $code --version) 2>$null | Select-Object -First 1)) -ForegroundColor DarkGray

$cfg           = Read-Jsonc -Path $extJson
$recommended   = @($cfg.recommendations)
$unwanted      = @($cfg.unwantedRecommendations)
$installed     = @((& $code --list-extensions) 2>$null | ForEach-Object { $_.Trim().ToLower() } | Where-Object { $_ })

Write-Host ("已装扩展  ：{0} 个" -f $installed.Count) -ForegroundColor DarkGray

$missing = @($recommended | Where-Object { $installed -notcontains $_.ToLower() })
$present = @($recommended | Where-Object { $installed -contains $_.ToLower() })
$junk    = @($unwanted    | Where-Object { $installed -contains $_.ToLower() })

Write-Host ''
Write-Host '================ 1. 推荐扩展：已装 ================' -ForegroundColor Green
if ($present.Count -eq 0) { Write-Host '  （无）' -ForegroundColor DarkGray }
foreach ($p in $present) { Write-Host ("  ✓ {0}" -f $p) -ForegroundColor Green }

Write-Host ''
Write-Host '================ 2. 推荐扩展：缺失 ================' -ForegroundColor Yellow
if ($missing.Count -eq 0) { Write-Host '  （无，已齐备）' -ForegroundColor DarkGray }
foreach ($m in $missing) { Write-Host ("  ✗ {0}" -f $m) -ForegroundColor Yellow }

Write-Host ''
Write-Host '================ 3. 与本项目无关的已装扩展 ================' -ForegroundColor Magenta
if ($junk.Count -eq 0) { Write-Host '  （无）' -ForegroundColor DarkGray }
foreach ($j in $junk) { Write-Host ("  ! {0}" -f $j) -ForegroundColor Magenta }

Write-Host ''
Write-Host ("汇总：已装推荐 {0} / 缺失 {1} / 无关 {2}" -f $present.Count, $missing.Count, $junk.Count) -ForegroundColor Cyan

# ---------------------------- 安装 ----------------------------
if ($InstallRecommended) {
    if ($missing.Count -eq 0) {
        Write-Host ''
        Write-Host '无需安装，推荐扩展已齐备。' -ForegroundColor Green
    } else {
        Write-Host ''
        Write-Host '开始安装缺失的推荐扩展…' -ForegroundColor Cyan
        foreach ($m in $missing) {
            Write-Host ("  → 安装 {0}" -f $m) -ForegroundColor Cyan
            & $code --install-extension $m --force 2>&1 | ForEach-Object { Write-Host ("      {0}" -f $_) -ForegroundColor DarkGray }
        }
        Write-Host '安装流程结束。若刚装 cpptools，请执行 task「Solstice: 生成工程文件 (Solution)」。' -ForegroundColor Green
    }
}

# ---------------------------- 卸载 ----------------------------
if ($RemoveUnwanted) {
    if ($junk.Count -eq 0) {
        Write-Host ''
        Write-Host '没有需要卸载的扩展。' -ForegroundColor Green
    } else {
        Write-Host ''
        if (-not $Confirm) {
            Write-Host '以下扩展将被卸载（预览模式，未执行）：' -ForegroundColor Yellow
            foreach ($j in $junk) { Write-Host ("  - {0}" -f $j) -ForegroundColor Yellow }
            Write-Host ''
            Write-Host '确认请追加 -Confirm 参数，例如：' -ForegroundColor Yellow
            Write-Host '  powershell -ExecutionPolicy Bypass -File tools\env\vscode-extensions.ps1 -RemoveUnwanted -Confirm' -ForegroundColor DarkGray
            Write-Host ''
            Write-Host '⚠ 这些扩展是【全局卸载】—— 会影响你在其它项目里的使用。' -ForegroundColor Red
            Write-Host '  若它们还有别的用途，建议改用 VSCode 的「工作区禁用扩展」：' -ForegroundColor DarkGray
            Write-Host '  扩展面板 → 右键扩展 → Disable (Workspace)' -ForegroundColor DarkGray
        } else {
            Write-Host '开始卸载与本项目无关的扩展…' -ForegroundColor Magenta

            # 扩展之间存在依赖（例：vscjava.vscode-java-test → redhat.java），
            # 被依赖的扩展在其依赖仍存在时无法卸载，且单个失败不应中断整批。
            # 故：多轮尝试，每轮只卸还卸得掉的，直到没有进展；最后报告残留。
            $pending = @($junk)
            $round = 0
            $prevEap = $ErrorActionPreference
            $ErrorActionPreference = 'Continue'   # 原生命令（code.cmd）失败不应终止整批
            try {
                while ($pending.Count -gt 0) {
                    $round++
                    $progress = $false
                    $next = @()
                    foreach ($j in $pending) {
                        Write-Host ("  → 卸载 {0}" -f $j) -ForegroundColor Magenta
                        $out = (& $code --uninstall-extension $j 2>&1 | Out-String).Trim()
                        if ($LASTEXITCODE -eq 0 -and $out -notmatch 'Cannot uninstall') {
                            Write-Host '      ✓ 已卸载' -ForegroundColor DarkGray
                            $progress = $true
                        } else {
                            $next += $j
                        }
                    }
                    $pending = @($next)
                    if (-not $progress) { break }
                }
            } finally {
                $ErrorActionPreference = $prevEap
            }

            # VS Code CLI 的卸载是异步落盘的，单轮返回值可能偏保守；
            # 以真实已装列表复核，避免把「其实已卸载成功」的项误报为残留。
            $stillInstalled = @((& $code --list-extensions) 2>$null | ForEach-Object { $_.Trim().ToLower() } | Where-Object { $_ })
            $pending = @($pending | Where-Object { $stillInstalled -contains $_.ToLower() })

            if ($round -gt 1) { Write-Host ("  （依赖关系需要多轮卸载，共 {0} 轮）" -f $round) -ForegroundColor DarkGray }

            if ($pending.Count -gt 0) {
                Write-Host ''
                Write-Host ("⚠ 仍有 {0} 个未能卸载（多为被其它扩展依赖）：" -f $pending.Count) -ForegroundColor Yellow
                foreach ($j in $pending) { Write-Host ("  - {0}" -f $j) -ForegroundColor Yellow }
                Write-Host '  处理建议：在扩展面板先卸掉依赖它的扩展，再重跑本脚本。' -ForegroundColor DarkGray
            } else {
                Write-Host '卸载完成。' -ForegroundColor Green
            }
        }
    }
}

Write-Host ''
Write-Host '提示：扩展清单的唯一真源是 .vscode\extensions.json，改那里即可。' -ForegroundColor DarkGray
exit 0

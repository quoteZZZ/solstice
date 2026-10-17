<#
.SYNOPSIS
  Copy generated media from studio/out into game/Content so the engine can see it.

.DESCRIPTION
  REVIEW R-24: the generator drops every image in studio/out/ and nothing ever moved them into
  game/Content/, so the manifest could list dozens of textures that the engine had no way to
  reference. This is the missing delivery bridge.

  It reads studio/out/manifest.jsonl (the compliance record) and copies each entry to
  game/Content/<base>/<basename>, where <base> follows the manifest "kind":

      card                        -> Cards
      ui                          -> UI
      texture / cutout / fx / bg  -> World

  Idempotent by content: an entry is skipped when the destination already hashes to the sha256
  the manifest recorded, so a second run copies nothing and does not touch a single mtime.

.PARAMETER Manifest
  Default studio\out\manifest.jsonl.

.PARAMETER ContentRoot
  Default game\Content.

.PARAMETER DryRun
  Report what would be copied and change nothing.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\build\import-assets.ps1 -DryRun
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\build\import-assets.ps1
#>
[CmdletBinding()]
param(
    [string]$Manifest,
    [string]$ContentRoot,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

# tools/build -> repo root
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

if ([string]::IsNullOrWhiteSpace($Manifest)) {
    $Manifest = Join-Path $repoRoot 'studio\out\manifest.jsonl'
}
if ([string]::IsNullOrWhiteSpace($ContentRoot)) {
    $ContentRoot = Join-Path $repoRoot 'game\Content'
}

if (-not (Test-Path -LiteralPath $Manifest)) {
    Write-Host ('[import] manifest not found: {0}' -f $Manifest) -ForegroundColor Red
    exit 1
}
if (-not (Test-Path -LiteralPath $ContentRoot)) {
    Write-Host ('[import] content root not found: {0}' -f $ContentRoot) -ForegroundColor Red
    exit 1
}

# manifest kind -> folder under game/Content
$FolderByKind = @{
    'card'    = 'Cards'
    'ui'      = 'UI'
    'texture' = 'World'
    'cutout'  = 'World'
    'fx'      = 'World'
    'bg'      = 'World'
}

$studioRoot = Join-Path $repoRoot 'studio'

$copied = 0
$skipped = 0
$missing = 0
$unknown = 0

foreach ($line in [System.IO.File]::ReadAllLines($Manifest)) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    $entry = $null
    try { $entry = $line | ConvertFrom-Json } catch { continue }
    if (-not $entry.file) { continue }

    $kind = [string]$entry.kind
    if (-not $FolderByKind.ContainsKey($kind)) {
        Write-Host ('  ?  unknown kind "{0}" for {1} - skipped' -f $kind, $entry.file) -ForegroundColor DarkYellow
        $unknown++
        continue
    }

    # manifest paths are relative to studio/
    $source = Join-Path $studioRoot ([string]$entry.file)
    if (-not (Test-Path -LiteralPath $source)) {
        Write-Host ('  !  missing source: {0}' -f $entry.file) -ForegroundColor Red
        $missing++
        continue
    }

    $targetDir = Join-Path (Join-Path $ContentRoot $FolderByKind[$kind]) ''
    $target = Join-Path $targetDir (Split-Path -Leaf $source)

    $same = $false
    if (Test-Path -LiteralPath $target) {
        $recorded = [string]$entry.sha256
        if ([string]::IsNullOrWhiteSpace($recorded)) {
            $same = ((Get-Item -LiteralPath $target).Length -eq (Get-Item -LiteralPath $source).Length)
        } else {
            $same = ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -eq $recorded.ToLowerInvariant())
        }
    }

    if ($same) {
        $skipped++
        continue
    }

    if ($DryRun) {
        Write-Host ('  +  would copy {0} -> Content\{1}\{2}' -f $entry.file, $FolderByKind[$kind], (Split-Path -Leaf $source)) -ForegroundColor Cyan
    } else {
        if (-not (Test-Path -LiteralPath $targetDir)) {
            New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
        }
        Copy-Item -LiteralPath $source -Destination $target -Force
    }
    $copied++
}

$mode = if ($DryRun) { 'dry run' } else { 'imported' }
Write-Host ('[import] {0}: {1} copied, {2} already current, {3} missing source, {4} unknown kind' -f $mode, $copied, $skipped, $missing, $unknown) -ForegroundColor Green

if ($missing -gt 0) { exit 1 }
exit 0

# Installs the Solstice pre-commit hook for this clone.
# ASCII-only on purpose: no encoding concerns under Windows PowerShell 5.1 / PS 7+.
# Idempotent: safe to run repeatedly. Uninstall: git config --unset core.hooksPath
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..\..')

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Host '[install-hooks] git not found - install Git first.' -ForegroundColor Red
    exit 1
}
if (-not (Test-Path 'tools\hooks\pre-commit')) {
    Write-Host '[install-hooks] tools\hooks\pre-commit is missing.' -ForegroundColor Red
    exit 1
}

git config core.hooksPath tools/hooks
Write-Host '[install-hooks] core.hooksPath = tools/hooks'
Write-Host '[install-hooks] pre-commit now runs tools/checks/check-links.py + tools/checks/validate-contracts.py.'

# Smoke-test the two checks right away (same exit codes the hook will enforce).
python tools/checks/check-links.py
if ($LASTEXITCODE -ne 0) {
    Write-Host '[install-hooks] check-links.py FAILED - fix before committing.' -ForegroundColor Red
    exit 1
}
python tools/checks/validate-contracts.py
if ($LASTEXITCODE -ne 0) {
    Write-Host '[install-hooks] validate-contracts.py FAILED - fix before committing.' -ForegroundColor Red
    exit 1
}

Write-Host '[install-hooks] hook installed and both self-checks pass.' -ForegroundColor Green

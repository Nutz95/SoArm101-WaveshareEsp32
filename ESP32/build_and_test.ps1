param(
  [switch]$SkipStructural,
  [switch]$SkipFollowerBuild,
  [switch]$SkipNativeTests
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

function Invoke-Step {
  param(
    [string]$Name,
    [scriptblock]$Action
  )

  Write-Host "==> $Name" -ForegroundColor Cyan
  & $Action
  if ($LASTEXITCODE -ne 0) {
    throw "$Name failed with exit code $LASTEXITCODE"
  }
}

if (-not $SkipStructural) {
  Invoke-Step "Structural limits check" {
    python tools/check_structural_limits.py --project-root .
  }
}

Invoke-Step "Build leader firmware" {
  pio run
}

if (-not $SkipFollowerBuild) {
  Invoke-Step "Build follower-ota firmware" {
    pio run -e follower-ota
  }
}

if (-not $SkipNativeTests) {
  Invoke-Step "Run native tests" {
    pio test -e native
  }
}

Write-Host "All selected build/test steps succeeded." -ForegroundColor Green

<#
Build both firmware environments (leader and follower) in one PlatformIO invocation.
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Push-Location $scriptDir

function Get-PlatformIoCommand {
    $pioCommand = Get-Command pio -ErrorAction SilentlyContinue
    if ($pioCommand) {
        return $pioCommand.Source
    }

    $platformIoCommand = Get-Command platformio -ErrorAction SilentlyContinue
    if ($platformIoCommand) {
        return $platformIoCommand.Source
    }

    throw "PlatformIO CLI not found in PATH. Install PlatformIO Core and retry."
}

try {
    $pio = Get-PlatformIoCommand
    Write-Host "[build_all] Using command: $pio"

    & $pio run -e leader -e follower
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for one or more environments (exit $LASTEXITCODE)."
    }

    Write-Host "[build_all] Both builds finished." -ForegroundColor Green
} catch {
    Write-Host "[build_all] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}

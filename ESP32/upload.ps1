<#
Upload leader, follower, or both firmware images through USB.
#>

param(
    [ValidateSet('leader', 'follower', 'both')]
    [string] $Target = 'both',

    [string] $LeaderPort = 'COM7',
    [string] $FollowerPort = 'COM8',

    [switch] $NoBuild
)

Set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Push-Location $scriptDir

function Get-PlatformIoCommand {
    $pioCommand = Get-Command pio -ErrorAction SilentlyContinue
    if ($pioCommand) {
        return 'pio'
    }

    $platformIoCommand = Get-Command platformio -ErrorAction SilentlyContinue
    if ($platformIoCommand) {
        return 'platformio'
    }

    throw "PlatformIO CLI not found in PATH. Install PlatformIO Core and retry."
}

function Invoke-Upload {
    param(
        [string] $PioCommand,
        [string] $EnvironmentName,
        [string] $Port
    )

    Write-Host "[upload] Uploading environment '$EnvironmentName' on port $Port"
    $uploadArguments = @('run', '-e', $EnvironmentName)
    if ($NoBuild.IsPresent) {
        $uploadArguments += @('-t', 'nobuild')
    }

    $uploadArguments += @('-t', 'upload', '--upload-port', $Port)

    & $PioCommand @uploadArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Upload failed for environment '$EnvironmentName' on port $Port."
    }
}

try {
    $pio = Get-PlatformIoCommand
    Write-Host "[upload] Using command: $pio"

    if ($Target -eq 'leader' -or $Target -eq 'both') {
        Invoke-Upload -PioCommand $pio -EnvironmentName 'leader' -Port $LeaderPort
    }

    if ($Target -eq 'follower' -or $Target -eq 'both') {
        Invoke-Upload -PioCommand $pio -EnvironmentName 'follower' -Port $FollowerPort
    }

    Write-Host "[upload] Upload completed." -ForegroundColor Green
} catch {
    Write-Host "[upload] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}

<#
.SYNOPSIS
    Build and upload firmware to the FOLLOWER board.

.PARAMETER Ota
    Upload via OTA (WiFi) instead of USB.
    Requires: SOARM_WIFI_SSID and SOARM_WIFI_PASS env vars set,
              board powered and connected to WiFi.

.PARAMETER NoBuild
    Skip the build step; only upload the existing .bin file.

.PARAMETER Port
    USB serial port for USB upload. Default: COM8.

.EXAMPLE
    .\build_upload_follower.ps1              # USB build + upload
    .\build_upload_follower.ps1 -Ota         # OTA build + upload
    .\build_upload_follower.ps1 -NoBuild     # USB upload only
    .\build_upload_follower.ps1 -Ota -NoBuild  # OTA upload only
#>
param(
    [switch]$Ota,
    [switch]$NoBuild,
    [string]$Port = "COM8",
    [string]$OtaIp = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path $MyInvocation.MyCommand.Path
$pio       = "pio"

if ($Ota) {
    $env_name = "follower-ota"
} else {
    $env_name = "follower"
}

Write-Host "=== SoArm101 - Follower firmware ===" -ForegroundColor Cyan
Write-Host "Environment : $env_name"
if (-not $Ota) { Write-Host "Port        : $Port" }
if ($Ota -and $OtaIp -ne "") { Write-Host "OTA IP      : $OtaIp" }
Write-Host ""

# Override upload port for USB mode when user specifies a custom port.
if (-not $Ota -and $Port -ne "COM8") {
    $env:PLATFORMIO_UPLOAD_PORT = $Port
}
if ($Ota -and $OtaIp -ne "") {
    $env:PLATFORMIO_UPLOAD_PORT = $OtaIp
}

# Build step.
if (-not $NoBuild) {
    Write-Host "--- Building ---" -ForegroundColor Yellow
    & $pio run -e $env_name
    if ($LASTEXITCODE -ne 0) {
        Write-Host "BUILD FAILED (exit $LASTEXITCODE)" -ForegroundColor Red
        exit $LASTEXITCODE
    }
    Write-Host "Build OK" -ForegroundColor Green
    Write-Host ""
}

# Upload step.
Write-Host "--- Uploading ---" -ForegroundColor Yellow
& $pio run -e $env_name -t nobuild -t upload

if ($LASTEXITCODE -ne 0) {
    Write-Host "UPLOAD FAILED (exit $LASTEXITCODE)" -ForegroundColor Red
    if ($Ota) {
        Write-Host "OTA tips:" -ForegroundColor Yellow
        Write-Host "  - Is the board powered and on the same WiFi?"
        Write-Host "  - Check SOARM_WIFI_SSID / SOARM_WIFI_PASS env vars."
        Write-Host "  - If mDNS fails, use -OtaIp 192.168.x.y"
        Write-Host "  - Or set upload_port = 192.168.x.y in [env:follower-ota]"
    } else {
        Write-Host "USB tips:" -ForegroundColor Yellow
        Write-Host "  - If 'Wrong boot mode', hold BOOT, tap EN, release BOOT, retry."
    }
    exit $LASTEXITCODE
}

Write-Host "Upload OK" -ForegroundColor Green

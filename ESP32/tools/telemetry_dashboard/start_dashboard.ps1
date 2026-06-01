param(
  [string]$LeaderHost = "soarm-leader.local",
  [int]$LeaderPort = 9090,
  [int]$DashboardPort = 8080,
  [string]$LeaderSerial = "",
  [int]$LeaderSerialBaud = 0,
  [string]$FollowerCom = "",
  [switch]$UseWifiLeader,
  [switch]$EnableComMirror
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$pioPorts = & python (Join-Path $scriptDir "resolve_pio_ports.py") | ConvertFrom-Json

if ([string]::IsNullOrWhiteSpace($LeaderSerial)) {
  $LeaderSerial = [string]$pioPorts.leader_port
}
if ($LeaderSerialBaud -le 0) {
  $LeaderSerialBaud = [int]$pioPorts.leader_baud
}
if ([string]::IsNullOrWhiteSpace($FollowerCom)) {
  $FollowerCom = [string]$pioPorts.follower_port
}

$argsList = @(
  ".\telemetry_dashboard.py",
  "--dashboard-port", $DashboardPort
)

if ($UseWifiLeader -or [string]::IsNullOrWhiteSpace($LeaderSerial)) {
  $argsList += @("--leader-host", $LeaderHost, "--leader-port", $LeaderPort)
} else {
  $argsList += @("--leader-serial", $LeaderSerial, "--leader-serial-baud", $LeaderSerialBaud)
}

if ($EnableComMirror -and -not [string]::IsNullOrWhiteSpace($FollowerCom)) {
  $argsList += @("--enable-com-mirror", "--follower-com", $FollowerCom)
}

Push-Location $scriptDir
try {
  Write-Host "Dashboard: http://127.0.0.1:$DashboardPort"
  if ($argsList -contains "--leader-serial") {
    Write-Host "Leader source: USB $($LeaderSerial) @ $($LeaderSerialBaud) (from platformio.ini)"
  } else {
    Write-Host "Leader source: Wi-Fi $LeaderHost`:$LeaderPort"
  }
  python @argsList
}
finally {
  Pop-Location
}

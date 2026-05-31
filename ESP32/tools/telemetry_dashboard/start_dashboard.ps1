param(
  [string]$LeaderHost = "soarm-leader.local",
  [int]$LeaderPort = 9090,
  [int]$DashboardPort = 8080,
  [string]$FollowerCom = "COM8",
  [switch]$AutoStartComMirror
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$argsList = @(
  ".\telemetry_dashboard.py",
  "--leader-host", $LeaderHost,
  "--leader-port", $LeaderPort,
  "--dashboard-port", $DashboardPort,
  "--enable-com-mirror",
  "--follower-com", $FollowerCom
)

if ($AutoStartComMirror) {
  $argsList += "--auto-start-com-mirror"
}

Push-Location $scriptDir
try {
  python @argsList
}
finally {
  Pop-Location
}

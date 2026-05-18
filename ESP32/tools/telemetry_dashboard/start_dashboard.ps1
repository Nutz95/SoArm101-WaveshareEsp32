param(
  [string]$LeaderHost = "soarm-leader.local",
  [int]$LeaderPort = 9090,
  [int]$DashboardPort = 8080
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Push-Location $scriptDir
try {
  python .\telemetry_dashboard.py --leader-host $LeaderHost --leader-port $LeaderPort --dashboard-port $DashboardPort
}
finally {
  Pop-Location
}

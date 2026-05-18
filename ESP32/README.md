# ESP32 Firmware

This folder contains the dual-board firmware scaffold for:
- Leader board (OLED + Bluetooth + dual status LED)
- Follower board (servo mirror receiver)

## PlatformIO Initialization

No extra initialization command is required because `platformio.ini` is already present.

---

## WiFi Credentials Setup

WiFi credentials are **never stored in the repository**. They are injected from OS
environment variables at build time.

Set them in your PowerShell session (or make them permanent via System Properties):

```powershell
$env:SOARM_WIFI_SSID = "YourNetwork"
$env:SOARM_WIFI_PASS = "YourPassword"
```

To make them permanent (survive reboots):
```powershell
[System.Environment]::SetEnvironmentVariable("SOARM_WIFI_SSID", "YourNetwork", "User")
[System.Environment]::SetEnvironmentVariable("SOARM_WIFI_PASS", "YourPassword",  "User")
```

Once set, every `pio run` / `.\build_upload_*.ps1` invocation picks them up automatically.

Optional helper variable for leader OLED display:

```powershell
$env:SOARM_FOLLOWER_OTA_IP = "192.168.1.73"
```

This value is shown on the leader OLED as the follower IP hint.

---

## Uploading Firmware

### Method 1 — USB (first flash or recovery)

Use this method the very first time, or when a board can't join WiFi.

```powershell
.\build_upload_leader.ps1          # build + upload leader via COM7
.\build_upload_follower.ps1        # build + upload follower via COM8
```

Custom port:

```powershell
.\build_upload_leader.ps1   -Port COM5
.\build_upload_follower.ps1 -Port COM9
```

Skip rebuild (upload existing binary):

```powershell
.\build_upload_leader.ps1   -NoBuild
.\build_upload_follower.ps1 -NoBuild
```

> **Tip:** If upload fails with `Wrong boot mode detected (0x13)`:
> 1. Hold `BOOT` on the board.
> 2. Press and release `EN` (reset) while holding `BOOT`.
> 3. Release `BOOT` and retry.

---

### Method 2 — OTA / WiFi (normal workflow after first flash)

After the first USB flash the boards join your WiFi network automatically and advertise
themselves via mDNS as `soarm-leader.local` and `soarm-follower.local`.  
From then on you can reprogram them **without touching a button**:

```powershell
.\build_upload_leader.ps1   -Ota    # build + OTA upload to soarm-leader.local
.\build_upload_follower.ps1 -Ota    # build + OTA upload to soarm-follower.local
```

You can force a direct IP target (skip mDNS) with `-OtaIp`:

```powershell
.\build_upload_leader.ps1   -Ota -OtaIp 192.168.1.42
.\build_upload_follower.ps1 -Ota -OtaIp 192.168.1.73
```

OTA upload only (no rebuild):

```powershell
.\build_upload_leader.ps1   -Ota -NoBuild
.\build_upload_follower.ps1 -Ota -NoBuild
```

If mDNS resolution fails (e.g. Windows firewall / Avahi not running), find the board's IP
on your router DHCP table and override it in `platformio.ini`:

```ini
[env:leader-ota]
upload_port = 192.168.1.42   ; replace with actual IP
```

### How to know both board IP addresses

1. Use your router DHCP leases page (most reliable).
2. Open serial monitor (`pio device monitor -b 115200`) and look for:
	- `[WiFi] connected ip=...` on each board.
3. On the leader OLED, 4 lines are shown:
	- Line 1: leader IP (`L:`)
	- Line 2: follower IP hint (`F:`)
	- Line 3: current mode (`M:`)
	- Line 4: current status (`S:`)

---

## Build All Environments (no upload)

```powershell
.\build_all.ps1
```

Builds both `leader` and `follower` environments in one pass.

---

## Unit Tests (Host)

```powershell
pio test -e native
```

Validates pure shared logic (state machine) without hardware peripherals.

---

## ESP-NOW Pairing Security

Leader and follower now perform first-contact pairing and store peer MAC addresses
in NVS. After pairing:

1. Leader accepts presence frames only from the paired follower MAC.
2. Follower sends telemetry/presence only to the paired leader MAC.
3. Nearby ESP-NOW devices are ignored unless pairing is reset.

Pairing keys are stored under namespace `soarm-pair` in NVS (`leader_mac`,
`follower_mac`).

---

## External Telemetry Dashboard (Python)

The ESP exposes a binary command and telemetry socket on leader port `9090`.

Launch the local dashboard:

```powershell
.\tools\telemetry_dashboard\start_dashboard.ps1 -LeaderHost soarm-leader.local
```

This starts:

1. A TCP client to `soarm-leader.local:9090`.
2. A local web UI at `http://127.0.0.1:8080` with static files:
	- `tools/telemetry_dashboard/static/index.html`
	- `tools/telemetry_dashboard/static/app.js`
	- `tools/telemetry_dashboard/static/styles.css`

Stream behavior:

1. Python sends `StartStream` command to ESP.
2. ESP streams binary telemetry frames while client is connected.
3. On disconnect, stream stops automatically.


# ESP32 Firmware

This folder contains the dual-board firmware scaffold for:
- Leader board (OLED + Bluetooth + dual status LED)
- Follower board (servo mirror receiver)

## Architecture

The current message flow, board responsibilities, and telemetry path are documented in:

- [docs/architecture/README.md](docs/architecture/README.md)
- [docs/architecture/message-flow.svg](docs/architecture/message-flow.svg)
- [docs/teleop_performance.md](docs/teleop_performance.md) — teleop cadence, ESP-NOW vs Wi-Fi, LeRobot alignment
- [docs/teleop_radio_fluency.md](docs/teleop_radio_fluency.md) — smooth motion across modes, radio state, transitions

If you are changing command routing, pairing, or dashboard integration, read that architecture note first.

Additional guides:

- [docs/hardware.md](docs/hardware.md) — Waveshare boards, Xbox controller, follower board options
- [docs/networking.md](docs/networking.md) — router optional vs OTA/dashboard, `SOARM_WIFI_*` at build time
- [docs/calibration.md](docs/calibration.md) — joint min/max in NVS per arm

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

Servo inventory expectations are configured in:

- `src/Config/common_runtime_config.h`

Default values:

1. `EXPECTED_LEADER_SERVO_COUNT = 1`
2. `EXPECTED_FOLLOWER_SERVO_COUNT = 1`

Additional runtime tunables are grouped in:

- `src/Config/leader_runtime_config.h`
- `src/Config/follower_runtime_config.h`
- `src/Config/controller_mapping_config.h`

At startup, both boards run a local servo scan. The leader also requests a follower startup scan
once the ESP-NOW link is up. If a board count differs from expected, its status LED enters
red blinking `ServoFault` state.

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

Follower recovery for "never paired" state (erase pairing/NVS and reflash via USB):

```powershell
.\build_upload_follower.ps1 -FactoryResetPairing
```

Notes:

1. `-FactoryResetPairing` is USB-only (do not combine with `-Ota`).
2. It runs `pio run -e follower -t erase` before upload.
3. Use it when follower pairing appears stuck or mismatched with the leader.

> **Tip:** If upload fails with `Wrong boot mode detected (0x13)`:
> 1. Hold `BOOT` on the board.
> 2. Press and release `EN` (reset) while holding `BOOT`.
> 3. Release `BOOT` and retry.

> **Tip:** After flashing the follower, PlatformIO / the IDE may reuse COM8 for the next leader upload. The scripts pass `--upload-port` explicitly; see [docs/SALON_FLASH.md](docs/SALON_FLASH.md) for salon workflow (USB, OTA engage, Wi‑Fi direct without router).

---

### Method 2 — OTA / WiFi (normal workflow after first flash)

After the first USB flash the boards join your WiFi network automatically and advertise
themselves via mDNS as `soarm-leader.local` and `soarm-follower.local`.  
From then on you can reprogram them **without touching a button**:

1. Leader: cycle Xbox profile to **OTA**, press **A** (OLED `OTA ACTIVE`, router IP).
2. Then upload:

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

Important first pairing warning:

1. Power on leader and follower in an isolated environment for the first pairing
	(no other active ESP-NOW robots nearby).
2. The first valid `PairRequest` will lock the follower MAC on leader side.
3. Use dashboard `Reset Pairing` when you intentionally change robots.

CPU load telemetry uses FreeRTOS runtime stats. The project enables this through
`ESP32/sdkconfig.defaults` and `ESP32/platformio.ini` so the framework is built
with the required trace/runtime options.

---

## External Telemetry Dashboard (Python)

The ESP exposes a binary command and telemetry socket on leader port `9090`.

Launch the local dashboard:

```powershell
.\tools\telemetry_dashboard\start_dashboard.ps1
.\tools\telemetry_dashboard\start_dashboard.ps1 -LeaderHost soarm-leader.local
```

Behavior:

1. `start_dashboard.ps1` uses `soarm-leader.local` by default.
2. `-LeaderHost` overrides DNS/mDNS resolution for fixed-IP or custom host names.
3. UI `Pairing Control` panel displays leader MAC, paired follower MAC, pairing lock status,
	and provides `Reset Pairing` + `Servo Scan` command buttons.
4. UI `Servo Buses` panel shows leader/follower columns with detected ID chips,
	servo count, debug/manual flags, and telemetry text from each board.
5. UI `Servo Manual Commands` panel exposes debug enable/disable, move, set-id,
	and set-mode controls forwarded through leader command channel.
6. UI `Teleoperation` panel provides:
	- one-shot mirror for matched IDs (`TeleopMirror`, command `15`)
	- continuous mirror loop start/stop (`TeleopContinuousSet`, command `16`)
	- per-servo continuous mirror toggles using same-ID mapping
	- runtime indicator showing active continuous target (`all matched` or one servo ID)

Continuous teleop runtime notes:

1. Leader now runs a dedicated servo telemetry polling task.
2. Teleop mirror loop runs in a separate task and publishes mirrored positions without ACK wait.
3. Servo bus reads/writes are guarded by a lock to prevent read/write collisions.
4. `TeleopMirror` uses single-frame ESP-NOW send in loop mode to avoid burst amplification.

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

Legacy explicit launch (same result):

```powershell
.\tools\telemetry_dashboard\start_dashboard.ps1 -LeaderHost soarm-leader.local
```


# Networking — router, credentials, and router-less teleop

## Do you need a Wi-Fi router?

**No — not for core teleop.**

| Feature | Router required? |
|---------|------------------|
| **ESP-NOW teleop** (default) + Xbox | **No** — boards pair over ESP-NOW directly |
| **ESP-NOW turbo** | **No** |
| **Wi-Fi teleop** (leader soft-AP) | **No** — leader creates the AP; follower joins it |
| **Calibration** (Xbox workflow) | **No** |
| **Passthrough** (USB ↔ leader bus) | **No** |
| **OTA firmware update** | **Yes** (or USB flash) — boards must reach your LAN |
| **Python HTML dashboard** | **Yes** — laptop talks to leader at `leader-ip:9090` (or USB serial debug) |
| **Convenient channel alignment** | **Optional** — see [channel priming](#channel-priming-with-vs-without-router) |

Salon demo: power leader + follower + Xbox controller. No laptop, no router.

## What the router is for (when you have one)

A **travel router**, **phone hotspot**, or home AP is still very useful:

1. **OTA** — flash `leader-ota` / `follower-ota` without USB (`soarm-leader.local`, `soarm-follower.local`).
2. **Dashboard** — `telemetry_dashboard.py` streams telemetry and sends commands to the leader on TCP port **9090**.
3. **Channel priming** — both boards join the **same 2.4 GHz channel** as the router so ESP-NOW stays aligned (see [teleop_radio_fluency.md](teleop_radio_fluency.md)).
4. **OLED IP display** — leader shows assigned IP; optional `SOARM_FOLLOWER_OTA_IP` hint on OLED.

The router is **not** in the teleop data path for ESP-NOW mirror batches.

## Wi-Fi credentials at build time

Credentials are **compiled into the firmware** from environment variables (never committed to git).

### Set before build / upload

**PowerShell (Windows):**

```powershell
$env:SOARM_WIFI_SSID = "YourNetwork"
$env:SOARM_WIFI_PASS = "YourPassword"
# optional OLED hint on leader:
$env:SOARM_FOLLOWER_OTA_IP = "192.168.1.73"

cd ESP32
.\build_upload_leader.ps1 -UploadPort COM7
.\build_upload_follower.ps1 -UploadPort COM8
```

**Bash (Linux / macOS):**

```bash
export SOARM_WIFI_SSID="YourNetwork"
export SOARM_WIFI_PASS="YourPassword"
cd ESP32 && pio run -e leader -t upload
```

Permanent on Windows: System → Environment Variables → user variables `SOARM_WIFI_SSID`, `SOARM_WIFI_PASS`.

### How it reaches the firmware

`platformio.ini` passes them as compile flags:

```ini
-DWIFI_SSID=\"${sysenv.SOARM_WIFI_SSID}\"
-DWIFI_PASS=\"${sysenv.SOARM_WIFI_PASS}\"
```

Both **leader** and **follower** builds inherit `_wifi_flags`. Rebuild **both** after changing SSID/password.

### Router-less / no SSID configured

Leave variables **unset** or **empty** before build:

- `WifiOtaService` sets `staConnectDesired = false` and logs `SSID not configured`.
- Boards **do not** try to join a home network.
- **ESP-NOW pairing and teleop still work** if both radios end up on the same channel (typically default channel when unassociated).
- **OTA and Wi-Fi dashboard** need another way in (USB flash, USB serial dashboard).

### SSID configured but router powered off

Firmware still **attempts** STA during channel prime (~4 s), then **times out** and continues ESP-NOW. Behaviour is similar to router-less if both boards time out consistently. For best results in mixed setups, either bring the router up at boot or build without SSID for pure offline demos.

## OTA workflow (with router)

1. First flash via USB with SSID/password set.
2. Boards join LAN; hostnames `soarm-leader` / `soarm-follower` (mDNS).
3. Upload:

```powershell
.\build_upload_leader.ps1 -Ota
.\build_upload_follower.ps1 -Ota
```

Or Xbox profile **OTA ready** → **A** on leader, then OTA from IDE.

## Dashboard workflow (with router)

```powershell
cd ESP32/tools/telemetry_dashboard
.\start_dashboard.ps1
```

Browser: http://127.0.0.1:8080 — commands go to leader only.

USB alternative when Wi-Fi stream is paused (ESP-NOW teleop): `--leader-serial COM7` — see [xbox_ble_controls.md](architecture/xbox_ble_controls.md).

## Channel priming (with vs without router)

ESP-NOW needs leader and follower on the **same Wi-Fi channel**.

| Setup | Behaviour |
|-------|-----------|
| **Both on router** | Follower stays on router channel; leader **primes** (STA up ≤4 s), learns channel, suspends STA for ESP-NOW teleop. **Recommended** when a router is available. |
| **No router / no SSID** | Neither STA associates; after prime **timeout**, firmware refreshes ESP-NOW peers on the **current radio channel** (usually aligned if both boards are unassociated). **Salon-ready.** |
| **After Wi-Fi Direct teleop** | Temporary STA reconnect (~3 s) to re-align channel — needs SSID configured; if no SSID, timeout path still refreshes peers. |

Full state tables: [teleop_radio_fluency.md](teleop_radio_fluency.md).

## Checklist

| Goal | Router | SSID in build | Flash |
|------|--------|---------------|-------|
| Offline ESP-NOW demo | Off | Empty | USB once |
| Salon + OTA later | On at home | Set SSID | USB then OTA |
| Dashboard tuning | On | Set SSID | Either |

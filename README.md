# SoArm101 — Dual ESP32 Leader / Follower

Wireless teleoperation firmware for the **SO-ARM101** bimanual setup: a **leader** arm (operator + **Xbox Wireless Controller**) mirrors to a **follower** arm over **ESP-NOW** or **Wi-Fi** — **no Raspberry Pi on the arms**, and **no router required** for salon teleop.

Built for [**Waveshare ESP32**](ESP32/docs/hardware.md) servo driver boards and **Feetech STS3215** bus servos.

## Hardware (reference build)

| Role | Board | Controller |
|------|--------|------------|
| **Leader** | [Waveshare Servo Driver with ESP32](https://www.waveshare.com/servo-driver-with-esp32.htm) — OLED, LEDs, BLE | [Xbox Wireless Controller](https://www.xbox.com/accessories/controllers/xbox-wireless-controller) (Bluetooth) |
| **Follower** | [Waveshare Bus Servo Driver HAT (A)](https://www.waveshare.com/bus-servo-driver-hat-a.htm) | — |

Any Waveshare **ESP32 + STS3215** driver can run the follower firmware (including a second “leader” PCB — the follower build does not drive the OLED). Details: [ESP32/docs/hardware.md](ESP32/docs/hardware.md).

## Inspiration & lineage

| Resource | Role |
|----------|------|
| [**horndeer/SO-ARM101-LeRobot**](https://github.com/horndeer/SO-ARM101-LeRobot) | SO-ARM101 hardware reference, LeRobot-oriented bring-up, and documentation we used as a starting point |
| [**huggingface/lerobot**](https://github.com/huggingface/lerobot) | LeRobot library — datasets, policies, `lerobot-teleoperate`, training and eval tooling on a host PC |

We align the **servo bus path** with LeRobot (`sync_read` present position → remap → `sync_write` goals at ~60–83 Hz) but implement the **link layer** on embedded ESP32 firmware instead of USB serial to a Pi or laptop.

## Router optional — what needs what

| You want… | Router needed? |
|-----------|----------------|
| **ESP-NOW / turbo teleop + Xbox** | **No** |
| **Wi-Fi teleop** (leader soft-AP) | **No** |
| **Calibration** stored on each ESP | **No** |
| **OTA**, **HTML dashboard**, teleop **without Xbox**, easy channel align at boot | **Yes** (or USB alternatives) |

Wi-Fi credentials are baked in at **build time** via `SOARM_WIFI_SSID` / `SOARM_WIFI_PASS` — see [ESP32/docs/networking.md](ESP32/docs/networking.md). Leave them empty for a **router-less** demo flash.

## Why this project?

```text
  [ Xbox BLE ]     ESP-NOW / Wi-Fi        [ Follower arm ]
       │                  │                      │
  [ Leader ESP32 ] ───────────────────► STS servos ×6
       │
  optional: laptop dashboard (:9090) + travel router (OTA)
```

**Good fit when you want:**

- A **portable salon / demo** rig — teleop works **without a PC or router** (ESP-NOW + Xbox).
- **Low parts count** — two ESP32 boards + servos; add a hotspot only when you want OTA/dashboard.
- **Sub-100 ms mirroring** with a tuned ESP-NOW path and compact turbo codec.
- **Per-arm calibration in flash** (NVS) — no PC config file for joint ranges ([calibration doc](ESP32/docs/calibration.md)).

**Less suited when you need:**

- On-arm **cameras**, **dataset recording**, or **policy inference** — use LeRobot on a Pi/PC for that pipeline.
- Maximum **research flexibility** (HF Hub, multi-policy benchmarks) without a separate host.

## Operating modes (leader)

**Pick modes from the OLED menu** at boot (leader board with display). Full guide — layout, buttons, menu tree, and flows: **[ESP32/docs/oled_menu.md](ESP32/docs/oled_menu.md)**.

| Mode | What it does | Best for |
|------|----------------|----------|
| **ESP-NOW teleop** | ~83 Hz mirror, legacy batch packets | Exhibitions, lowest setup friction |
| **ESP-NOW turbo** | Same rate; 9–16 B sparse frames + OLED `lat` / `dr` | Bandwidth-tight or latency debugging |
| **ESP-NOW feedback** | *(planned)* Turbo downlink + load uplink; OLED `fb:xxHz` — [plan](ESP32/docs/teleop_espnow_feedback.md) |
| **Wi-Fi teleop** | Wi-Fi Direct soft-AP + UDP batches | Bench, **no router** link test |
| **Calibration** | Leader or follower min/max → NVS on each ESP | Joint range setup |
| **Passthrough** | USB serial ↔ leader servo bus (1 Mbaud) | Wired LeRobot-style tuning on leader |
| **OTA** | Flash firmware over Wi-Fi | Updates without USB (needs LAN) |

**D-pad** moves the menu cursor; **View (Mode)** moves down at the root menu only. **A** enters or confirms; **B** goes back or cancels teleop/calibration. See the [OLED menu guide](ESP32/docs/oled_menu.md) and [controller diagram](ESP32/docs/assets/XBoxControler.jpg).

**Fluency across mode changes** (channel priming, post–Wi-Fi resync, peer refresh): [ESP32/docs/teleop_radio_fluency.md](ESP32/docs/teleop_radio_fluency.md).

## Compared to LeRobot on Raspberry Pi

| | **This firmware (dual ESP32)** | **LeRobot + Pi (typical SO-101)** |
|--|--------------------------------|-----------------------------------|
| **Compute on arm** | ESP32 only | Raspberry Pi (or PC) per setup |
| **Teleop link** | ESP-NOW or Wi-Fi between boards | USB serial (`lerobot-teleoperate`) |
| **Host required for teleop** | No (ESP-NOW + Xbox) | Yes (Pi/PC runs Python) |
| **Router required for teleop** | No | Depends on setup (usually USB) |
| **Cameras & vision** | Not on-device | Native in LeRobot |
| **Record dataset / train policy** | Via host + LeRobot, not on ESP32 | First-class `lerobot-record`, `lerobot-train` |
| **Calibration** | NVS on each ESP32 | Host / LeRobot config |
| **Latency (mirror path)** | Tuned embedded path, turbo OLED metrics | USB-dependent; well understood in LeRobot |
| **Portability / salon** | Strong — power + Xbox only | Pi power, USB, optional cameras |
| **OTA to arms** | Built-in Wi-Fi OTA | Depends on Pi image / workflow |
| **Open ecosystem** | Firmware + dashboard in this repo | Hugging Face Hub, policies, docs |
| **Radio coexistence** | One 2.4 GHz radio (Wi-Fi + ESP-NOW + BLE) — mitigated in firmware | USB isolates teleop from Wi-Fi/BLE on arms |

**Practical split:** use **this repo** for robust **embedded mirroring and exhibition teleop**; use **[LeRobot](https://github.com/huggingface/lerobot)** when you **record demonstrations, train visuomotor policies, and evaluate on real robots**.

## Architecture (short)

- **Leader** — pairing authority, Xbox BLE, servo bus read, mirror encode, ESP-NOW / UDP send, telemetry server `:9090`.
- **Follower** — ESP-NOW / UDP receive, servo `SyncWritePosEx`, telemetry back via presence.
- **Dashboard** (optional) — `ESP32/tools/telemetry_dashboard/` on a laptop; never talks to the follower directly.

Details: [ESP32/docs/architecture/README.md](ESP32/docs/architecture/README.md) · [message flow diagram](ESP32/docs/architecture/message-flow.svg)

## Quick start

### 1. Wi-Fi credentials (optional, at build time)

Required for **OTA**, **dashboard over Wi-Fi**, and **teleop without Xbox**. **Skip** for router-less ESP-NOW-only demos.

```powershell
$env:SOARM_WIFI_SSID = "YourNetwork"
$env:SOARM_WIFI_PASS = "YourPassword"
```

Both boards must be built with the **same** SSID/password. Full explanation: [ESP32/docs/networking.md](ESP32/docs/networking.md).

### 2. Build & flash (USB)

```powershell
cd ESP32
.\build_upload_leader.ps1 -UploadPort COM7
.\build_upload_follower.ps1 -UploadPort COM8
```

### 3. ESP-NOW pairing (no dashboard required)

1. Power **leader** and **follower** within a few metres.
2. Wait **~5–15 s**: follower sends `PairRequest`, leader accepts automatically on first boot.
3. Serial / LEDs: `[FOLLOWER] Paired with leader` and leader stores follower MAC.

**No button press.** No router. No laptop.

To **re-pair** or **reset**: dashboard (Wi-Fi or USB serial), or `.\build_upload_follower.ps1 -FactoryResetPairing` — see [ESP32/docs/esp_now_pairing.md](ESP32/docs/esp_now_pairing.md).

### 4. Teleop

1. Pair **Xbox** to leader (BLE scan on each leader boot ~4 s).
2. On the **OLED menu**: **Teleop** → **ESP-NOW** (or Turbo) → **A** to start mirror → **B** to stop. Step-by-step: [ESP32/docs/oled_menu.md](ESP32/docs/oled_menu.md).

**Without Xbox:** use the HTML dashboard on the same LAN — [networking.md](ESP32/docs/networking.md).

Optional dashboard:

```powershell
cd ESP32/tools/telemetry_dashboard
.\start_dashboard.ps1
```

Open http://127.0.0.1:8080

Full firmware guide: [ESP32/README.md](ESP32/README.md)

## Documentation map

| Topic | Document |
|-------|----------|
| **Hardware & Xbox controller** | [ESP32/docs/hardware.md](ESP32/docs/hardware.md) |
| **ESP-NOW pairing (no dashboard)** | [ESP32/docs/esp_now_pairing.md](ESP32/docs/esp_now_pairing.md) |
| **Router optional, Wi-Fi env vars, OTA, dashboard teleop** | [ESP32/docs/networking.md](ESP32/docs/networking.md) |
| **Calibration in NVS (per arm)** | [ESP32/docs/calibration.md](ESP32/docs/calibration.md) |
| **Teleop fluency & radio states** | [ESP32/docs/teleop_radio_fluency.md](ESP32/docs/teleop_radio_fluency.md) |
| **OLED menu refactor (planned)** | [ESP32/docs/oled_menu_refactor_plan.md](ESP32/docs/oled_menu_refactor_plan.md) |
| Performance, LeRobot bus alignment | [ESP32/docs/teleop_performance.md](ESP32/docs/teleop_performance.md) |
| Turbo codec (v2 sparse/keyframe) | [ESP32/docs/teleop_espnow_turbo_codec.md](ESP32/docs/teleop_espnow_turbo_codec.md) |
| System architecture | [ESP32/docs/architecture/README.md](ESP32/docs/architecture/README.md) |
| **OLED interactive menu** | **[ESP32/docs/oled_menu.md](ESP32/docs/oled_menu.md)** |
| Xbox profiles & buttons | [ESP32/docs/architecture/xbox_ble_controls.md](ESP32/docs/architecture/xbox_ble_controls.md) |
| Leader flash recovery | [ESP32/docs/LEADER_FLASH_RECOVERY.md](ESP32/docs/LEADER_FLASH_RECOVERY.md) |
| Build, OTA, config | [ESP32/README.md](ESP32/README.md) |

## Tests & quality

```powershell
python ESP32/tools/check_structural_limits.py --project-root ESP32
cd ESP32
pio test -e native
pio run -e leader -e follower
```

## License

See repository license file. SO-ARM101 hardware and LeRobot are separate projects with their own terms — see links above.

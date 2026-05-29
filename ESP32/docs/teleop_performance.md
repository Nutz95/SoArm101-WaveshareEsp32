# Teleoperation Performance

This document describes how leader–follower mirroring is tuned, how it compares to LeRobot SO-101 USB teleop, and how to operate the system at exhibitions.

## Goals

| Goal | Approach |
|------|----------|
| LeRobot-like cadence | ~60 Hz control loop (`kTeleopTargetPeriodMs = 17`) |
| LeRobot-like bus I/O | `syncRead` present position + `SyncWritePosEx` goal batch |
| No fragile PC files | Calibration min/max in NVS profiles on each ESP32 |
| Salon robustness | Default **ESP-NOW** teleop; Wi-Fi for OTA + dashboard |
| Optional higher rate | Wi-Fi UDP fire-and-forget (v2 packets), future “turbo” profile |

## Data path (mirroring)

```text
Leader bus:  syncRead (6× position) → ServoPositionSnapshot
             ↓ remap calibration (per servo ID)
Mirror task: batch up to 6 IDs → ESP-NOW or Wi-Fi UDP (27 B)
Follower:    moveBatch SyncWritePosEx (no repeated EnableTorque in teleop)
```

Dashboard and OLED still use the text telemetry string (`#1 p2048;…`) built from the same sync read.

## Transport modes

### ESP-NOW (recommended for salon)

- Teleop batches on the existing presence peer link.
- ESP-NOW also carries follower IP, servo telemetry, and pairing.
- Wi-Fi stays up on both boards for OTA and the Python dashboard on the leader (`:9090`).
- Profile: Xbox cycle → **teleop ESP-NOW** (default profile `2` on boot).

### Wi-Fi UDP (bench / turbo experiments)

- Binary `BatchPacket` v2 on UDP port **29110** (follower listen), leader ACK port **29111**.
- Default: **fire-and-forget** (`kTeleopWifiRequireAck = false`) to avoid ACK traffic and lwIP `ENOMEM` (errno 12).
- Set `kTeleopWifiRequireAck = true` in `leader_runtime_config.h` for latency/debug measurements.
- Follower endpoint: presence IP from ESP-NOW first, else **`soarm-follower.local`** (mDNS).
- On send failure: 50 ms backoff + throttled serial log (Arduino core may still print `[WiFiUdp]` occasionally).

### Why not TCP for teleop?

TCP adds connection setup, retransmits, and head-of-line blocking. For 27-byte periodic position frames, **UDP** (or ESP-NOW) matches LeRobot’s “latest command wins” behaviour. TCP remains appropriate for dashboard commands and OTA.

## LeRobot comparison

| | LeRobot SO-101 | This firmware |
|--|----------------|---------------|
| Read | `bus.sync_read("Present_Position")` | `syncReadPacketTx/Rx` on STS present position |
| Write | `bus.sync_write("Goal_Position")` | `SyncWritePosEx` |
| Rate | 60 Hz default (`lerobot-teleoperate`) | 60 Hz target (`kTeleopTargetPeriodMs`) |
| Link | USB serial per arm | ESP-NOW or Wi-Fi UDP between ESP32s |

## Radio sharing (ESP-NOW + Wi-Fi)

Both use the same 2.4 GHz radio. Mitigations in firmware:

- Salon: teleop on ESP-NOW only; reduce unnecessary UDP teleop spam.
- Wi-Fi teleop bench: fire-and-forget UDP, larger lwIP pbuf pools (`sdkconfig.defaults`).
- Presence frames stay lower rate than teleop batches.

Future work (not all implemented yet): explicit **medium arbiter** task, presence throttling during Wi-Fi teleop, optional “turbo” profile above 60 Hz.

## Telemetry counters (dashboard)

| Field | Meaning |
|-------|---------|
| `teleop_mirror_latency_*` | RTT when ACK mode enabled (ESP-NOW or Wi-Fi ACK) |
| `teleop_mirror_pending_count` | Pending ACK slots (ACK mode) |
| `teleop_mirror_pending_count` (Wi-Fi no-ACK) | Mapped to **send fail** count for quick health check |
| `teleop_mirror_timeout_count` | ACK timeouts |

## Build / flash notes

Flash **leader and follower together** when changing teleop packet version (now **v2**).

```powershell
.\build_upload_leader.ps1 -Ota
.\build_upload_follower.ps1 -Ota
```

Structural check:

```powershell
python ESP32/tools/check_structural_limits.py --project-root ESP32
```

Host tests:

```powershell
cd ESP32
pio test -e native
```

## Operating checklist (salon)

1. Both boards on the same portable router (AP isolation off).
2. Xbox profile **teleop ESP-NOW**; press **A** to start mirror, **B** to stop.
3. Dashboard on laptop optional (`.\tools\telemetry_dashboard\start_dashboard.ps1` → http://127.0.0.1:8080).
4. If mirroring is choppy: check servo count LEDs, calibration validated, `teleop_mirror` send-fail counter on dashboard.

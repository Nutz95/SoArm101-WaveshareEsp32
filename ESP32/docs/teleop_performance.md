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

## ESP-NOW airtime (teleop vs presence)

During continuous teleop the follower must **not** send a dedicated command ACK and a full presence frame for every mirror batch (~60 Hz). That previously saturated the radio and caused:

- Serial log flood (`Presence`, `RX msgType=8` = `ServoControlBatch`)
- Stuttering / stepped follower curves on the dashboard
- Leader `[PAIR] Timeout` and follower IP `0.0.0.0`

Current behaviour:

| Traffic | Teleop active | Idle / commands |
|---------|---------------|-----------------|
| Mirror batch ACK | Staged into next periodic presence only | Full ACK burst (3×) for scans/moves |
| Presence period | 1000 ms | 250 ms |
| PairRequest while paired | 30 s | 5 s |

Leader re-pairs when it receives **Presence** or **PairRequest** from a follower after a timeout (no more `PairReset` on stale presence). Pairing timeout is **45 s** without presence when the watchdog is active.

During **calibration** (leader or follower profile, or range-capture phase), the pairing watchdog is **suspended** so a long min–max session does not drop the link. The follower sends **link keepalives** before and after `CalibrationCenter` (offset + grouped move).

### Leader center calibration (vs follower)

Leader center uses the same `calibrateOffsetsForDetectedServos()` as the follower, but the leader used to fail more often because:

1. `moveBatch()` was called while the bus lock was still held → move silently failed (`bus busy`).
2. The servo telemetry task kept polling the bus every ~17 ms during calibration modes.

Current behaviour: offsets for all reachable servos → **unlock** → one `SyncWrite` center move → relaxed verify (±400 counts, 25 attempts). Telemetry polling is **paused** on both boards during calibration modes.

## Planned: rest pose and leader “zero-G”

Not implemented in firmware yet; proposed workflow:

1. After min–max calibration, prompt **rest pose** capture (all servos) and store in NVS per arm (extend `CalibrationProfile`).
2. On teleop stop (dashboard **B** or UI stop): move both arms to **center** (`SyncWrite` batch), then to **rest** when within tolerance.
3. While teleop runs, monitor leader **present load / current** (STS register) — when user releases the leader and load drops, reduce follower torque or hold last pose to avoid the follower “falling”.

## Operating checklist (salon)

1. Both boards on the same portable router (AP isolation off).
2. Xbox profile **teleop ESP-NOW**; press **A** to start mirror, **B** to stop.
3. Dashboard on laptop optional (`.\tools\telemetry_dashboard\start_dashboard.ps1` → http://127.0.0.1:8080).
4. If mirroring is choppy: check servo count LEDs, calibration validated, `teleop_mirror` send-fail counter on dashboard.

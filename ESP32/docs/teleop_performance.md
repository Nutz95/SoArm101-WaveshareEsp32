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
| Presence period | 250 ms (teleop) | 250 ms |
| PairRequest while paired | 30 s | 5 s |

Leader re-pairs when it receives **Presence** or **PairRequest** from a follower after a timeout (no more `PairReset` on stale presence). Pairing timeout is **45 s** without presence when the watchdog is active.

During **calibration** (leader or follower profile, or range-capture phase), the pairing watchdog is **suspended** so a long min–max session does not drop the link. The follower sends **link keepalives** before and after `CalibrationCenter` (offset + grouped move) and after a successful **range capture** save.

### “Follower offline” with a valid IP after calibration

`isFollowerLinked()` requires a recent presence/ACK (`kPresenceTimeoutMs`, now **12 s**). A long follower center calibration can exceed that window while the IP string on the OLED stays valid. Fixes:

- Refresh `lastFollowerSeenMs` when the pairing watchdog **resumes** (calibration finished).
- `refreshFollowerLinkGrace()` after follower range commit on the leader.
- Follower keepalive after NVS capture.

### Wi-Fi teleop stepping

The follower **teleop_apply** FreeRTOS task (~17 ms) drains Wi-Fi/PC serial sockets with **latest-only** semantics and applies a single `moveBatch` per period. Bus telemetry publish on the follower is **paused** while teleop traffic is recent (`kTeleopTrafficRecentMs`). Wi-Fi mirror sends on **any** position change (`kTeleopMirrorMinPositionDeltaWifi = 0`). Leader caches the resolved follower IP to avoid repeated mDNS lookups. ESP-NOW presence is throttled to **5 s** while Wi-Fi batches are active so the radio can favor UDP.

COM mirror uses binary `leader_mirror_positions[]` in the Wi-Fi telemetry snapshot (not parsed text). Leader dashboard snapshots are throttled to **250 ms** during `PcSerialBridge` profile. See [architecture/communication_links.md](architecture/communication_links.md).

**Refactor:** profile 5 and COM mirror are scheduled for removal. See [REFACTORING_PLAN.md](REFACTORING_PLAN.md).

If motion is still choppy on Wi-Fi but smooth on **PC serial bridge** (profile 5), the bottleneck is Wi-Fi/radio—not the 60 Hz pipeline.

## PC serial bench mode (profile 5, TeleopPcSerial)

Same mirror logic as Wi-Fi/ESP-NOW; batch frames are written on **USB CDC Serial** (UART0 via CP2102 on each board).

There is **no GPIO wiring** between the two arms. Both boards stay connected to the PC; a COM bridge forwards leader USB → follower USB.

### Setup

1. Connect **follower** to the PC USB port (e.g. COM8). Leader USB is **not** required.
2. Install pyserial: `pip install pyserial`
3. Start the dashboard with COM mirror enabled:

```powershell
python tools/telemetry_dashboard/telemetry_dashboard.py --enable-com-mirror --follower-com COM8
```

Or use `.\tools\telemetry_dashboard\start_dashboard.ps1 -FollowerCom COM8`.

4. In the web UI **Modes** tab: **Start COM mirror** (watch the console for `[com-mirror] OPEN OK`).
5. Apply profile **PC serial bridge** (profile 5), then **Start Continuous** (Teleop tab) or press **A** on the controller.
6. **Packets sent** in the UI should increase; **State** should show `sending`.

The dashboard reads leader servo positions over Wi-Fi telemetry and writes binary batch frames to the follower COM port. ESP-NOW is not used for mirror batches in this mode.

## Leader USB passthrough (wired LeRobot on leader arm)

Profile **4** (Xbox mode cycle or dashboard **Modes** tab → Passthrough):

- USB serial reopens at **1 000 000** baud and is bridged byte-for-byte to `Serial2` (servo bus).
- Servo telemetry and mirror tasks stay idle; set the host terminal to **1000000** baud.
- The follower arm still uses its hardware UART switch for direct bus access.

### Leader center calibration (vs follower)

Leader center uses the same `calibrateOffsetsForDetectedServos()` as the follower, but the leader used to fail more often because:

1. `moveBatch()` was called while the bus lock was still held → move silently failed (`bus busy`).
2. The servo telemetry task kept polling the bus every ~17 ms during calibration modes.

Current behaviour: offsets for all reachable servos → **unlock** → one `SyncWrite` center move → relaxed verify (±400 counts, 25 attempts). Telemetry polling is **paused** on both boards during calibration modes.

### Xbox cal follower (fixed)

One **A** press no longer runs center calibration **and** commit in the same frame. Cycle to cal follower → OLED **cal follower? press A** → first **A** sends center to follower (wait for **cal follower move extremes**) → move arm to min/max → second **A** commits. **B** cancels.

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

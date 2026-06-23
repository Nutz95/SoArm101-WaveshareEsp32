# ESP-NOW Turbo Teleop — Codec & Optimization Plan

This document tracks the segmented optimization of **TeleopEspNowTurbo** only. Classic **TeleopEspNow** keeps the legacy `PresencePacket` batch path as the performance reference.

## Why turbo stuttered vs classic (root causes)

| Factor | Classic ESP-NOW | Turbo (before fixes) |
|--------|-----------------|----------------------|
| Position source | Telemetry task refreshes snapshot every **12 ms** | Snapshot rafraîchi **80 ms** while mirror loop ran faster → goals repeated then jumped |
| Wire payload | Full **172 B** `PresencePacket` per batch | Was 172 B; now **9–16 B** (v2 sparse/keyframe) |
| Delta filter | `minPositionDelta = 1` | Now `2` + direct bus read per mirror frame |
| Follower apply | **12 ms** period | **12 ms** (aligned) |
| Servo count OLED | Stable | Partial sync read could report **F5** falsely (fixed) |

**Bench (user):** `lat 6–8 ms`, `dr0` — ESP-NOW send path healthy; jitter was stale goals + STS timing, not frame loss.

### Cold-boot ESP-NOW stutter (fixed)

After reboot, teleop could stutter until cycling Xbox profiles once. Root cause: leader **startup servo scan** pinned ESP-NOW peers to `WiFi.channel()` while home STA was still settling; profile cycling called `ensureEspNowTransportReady(0)` and suspended STA. Fix: never pin channel for routine commands, resync radio ~3.2 s after boot, suspend follower home STA when paired for salon ESP-NOW, refresh transport each tick on ESP-NOW profiles.

## Architecture (SOLID)

```text
TeleopMirrorBatchPayload (domain)
        │
        ▼
 ITeleopEspNowBatchCodec
   ├── TeleopEspNowLegacyBatchCodec        → 172 B PresencePacket (classic reference)
   └── TeleopEspNowTurboCompactCodec       → v2 sparse / keyframe wire format
           ├── TeleopEspNowTurboSession     → slot state (leader encode + follower decode)
           └── TeleopEspNowTurboKeyframePolicy → keyframe cadence only
```

- **Codecs** encode/decode bytes only.
- **Session** owns last-known absolute slot positions (IDs 1..6).
- **Keyframe policy** decides full resync vs sparse delta frame.
- **Transport** (`leader_presence_transport` / `follower_presence_inbound`) sends bytes and ingests payloads.

## Phase 1 — Done (compact 12-bit absolute)

- Superseded by **v2** sparse/keyframe wire format (v1 fixed 16 B packet removed).
- 6×12-bit STS positions (0–4095), `activeMask` for slot presence.
- Calibration remap on leader before encode; wire carries follower STS counts.

## Phase B — Done (sparse + keyframe, v2 wire)

### Wire format v2 (variable length)

| Field | Size | Notes |
|-------|------|-------|
| Header | **7 B** | `magic`, `version=2`, `messageType=12`, `activeMask`, `requestId` LE, `control` |
| Positions | **⌈popcount(mask)×12/8⌉ B** | 12-bit values in slot order (ID 1 = bit 0) |

**`control` byte:** bit **7** = keyframe, bits **0–6** = `speedPct` (0–100, 7 bits).

| Frame type | `activeMask` | Typical size (1 axis moving) |
|------------|--------------|------------------------------|
| **Keyframe** | all `knownMask` slots | up to **16 B** (7 + 9) |
| **Delta** | only axes in this mirror batch | down to **9 B** (7 + 2) for 1 servo |

**Keyframe triggers** (`teleop_espnow_turbo_config.h`):

- First frame / empty session
- Every **10** frames (`kTurboKeyframeEveryNFrames`)
- Every **125 ms** (`kTurboKeyframeIntervalMs`)

Leader holds `TeleopEspNowTurboSession turboEncodeSession_` (reset on mirror **A**).
Follower holds `turboDecodeSession_` (reset on teleop release).

### Tests (`test_teleop_espnow_codec`)

- Masked 12-bit pack/unpack
- v2 session round-trip
- Sparse delta smaller than keyframe
- Keyframe policy
- Wrong turbo wire version rejected
- Legacy 172 B path unchanged
- Remap + codec integration

## Phase C — Planned (per-joint range codes)

Map `[min,max]` from `CalibrationProfile` to fewer than 12 bits per joint when range is narrow.

## Phase D — Planned (signed delta on wire)

`int8` Δposition after keyframe (CAN-style); builds on phase B session state.

## Phase E — Planned (gripper haptic / force feedback)

**Goal:** when manipulating an object, read **gripper torque/load** on the follower and send it back to the leader to stiffen the leader gripper (haptic return).

```text
Leader teleop out (turbo v2) ──► Follower STS gripper
Follower load read ◄── periodic compact uplink (new message type, turbo-only)
Leader Xbox / leader gripper ──► stiffness or current overlay on operator hand
```

| Step | Work |
|------|------|
| E1 | Follower: poll STS **present load / current** on gripper servo during turbo |
| E2 | Compact ESP-NOW uplink (separate `TeleopGripperFeedback` packet, ~8–12 B) |
| E3 | Leader: map feedback → gripper torque / resistance on leader arm |
| E4 | Tunables: deadband, max stiffness, rate limit (salon-safe) |
| E5 | Unit tests for feedback codec + clamping (no hardware in native tests) |

Requires stable turbo downlink (phases 1 + B) before adding reverse traffic.

## Flash checklist

Turbo compact changes **wire format** for turbo only. Flash **leader + follower** together when updating phase 1+.

```powershell
python ESP32/tools/check_structural_limits.py --project-root ESP32
cd ESP32
pio test -e native
pio run -e leader -e follower
```

## Acceptance criteria

### Phase 1 + B

- [x] Classic ESP-NOW unchanged (legacy codec).
- [x] Turbo v2 sparse/keyframe on the wire.
- [x] `speedPct` in 7 bits; keyframe flag in bit 7.
- [x] `pio test -e native` green.
- [ ] Bench: turbo smooth; `dr` stays 0; airtime drops when few joints move.

### Phase E (future)

- [ ] Gripper load uplink without breaking teleop downlink cadence.
- [ ] Perceptible stiffening when follower gripper loads; no oscillation on release.

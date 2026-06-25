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

### Cold-boot ESP-NOW stutter (mitigated)

After reboot, teleop could stutter until cycling Xbox profiles once. Root causes:

1. Leader **suspended home STA at 1.2 s** before learning the router Wi-Fi channel; follower stayed on the router channel → ESP-NOW channel mismatch.
2. Leader **servo discovery scan** blocking the bus while mirror starts.
3. Classic ESP-NOW mirror reading a **stale telemetry snapshot** (turbo already used fast bus read).

Fixes: **channel priming** at boot; **post–Wi-Fi Direct resync**; fast bus read; no discovery scan while teleop armed. Full radio/state narrative: [teleop_radio_fluency.md](teleop_radio_fluency.md).

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

## Phase E — Feedback teleop (see dedicated plan)

**Goal:** gripper / multi-joint **load feedback** over ESP-NOW, built on turbo downlink.

Full phased spec (OLED display → leader haptic): **[teleop_espnow_feedback.md](teleop_espnow_feedback.md)**.

Summary:

```text
Leader teleop out (turbo v2) ──► Follower STS
Follower load read ◄── compact uplink (new message type)
Leader OLED + (later) leader torque overlay
```

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

See [teleop_espnow_feedback.md](teleop_espnow_feedback.md).

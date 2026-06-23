# ESP-NOW Turbo Teleop — Codec & Optimization Plan

This document tracks the segmented optimization of **TeleopEspNowTurbo** only. Classic **TeleopEspNow** keeps the legacy `PresencePacket` batch path as the performance reference.

## Why turbo stuttered vs classic (root causes)

| Factor | Classic ESP-NOW | Turbo (before fixes) |
|--------|-----------------|----------------------|
| Position source | Telemetry task refreshes snapshot every **12 ms** | Mirror loop read snapshot while telemetry slowed to **80 ms** → same goal re-sent, then **jumps** |
| Wire payload | Full **172 B** `PresencePacket` per batch | Same 172 B (airtime overhead, not `dr` limited) |
| Delta filter | `minPositionDelta = 1` | Was `-1` (all servos every frame), then `2` + own bus read |
| Follower apply | **12 ms** period | **10 ms** (could pile `SyncWrite` on STS bus) |
| Servo count OLED | Stable | Partial sync read could report **F5** falsely |

**Current metrics (user bench):** `lat 6–8 ms`, `dr0` → mirror loop and ESP-NOW send are healthy; remaining jitter was dominated by **stale goals** and **follower bus timing**, not frame loss.

## Architecture (SOLID)

```text
TeleopMirrorBatchPayload (domain)
        │
        ▼
 ITeleopEspNowBatchCodec
   ├── TeleopEspNowLegacyBatchCodec   → PresencePacket ServoControlBatch (172 B) — classic reference
   └── TeleopEspNowTurboCompactCodec → TeleopEspNowTurboPacket (17 B) — turbo only
```

- **Single responsibility:** codecs only encode/decode; transport only sends bytes; mirror task only builds payloads.
- **Open/closed:** new wire formats add a codec implementation, not a fork of mirror logic.
- **Liskov:** both codecs implement the same `encode` / `decode` / `encodedSize`.
- **KISS / YAGNI:** phase 1 = compact absolute frame + 12-bit slots; no zlib, no delta-on-wire yet.

## Phase 1 — Implemented (this change)

### Wire format: `TeleopEspNowTurboPacket` (16 bytes)

| Offset | Field | Notes |
|--------|-------|-------|
| 0 | `magic` | `0xA5` (`kPresenceMagic`) |
| 1 | `version` | `1` (`kTeleopEspNowTurboPacketVersion`) |
| 2 | `messageType` | `12` (`TeleopMirrorCompact`) |
| 3 | `activeMask` | bit *i* → servo ID *i+1* present |
| 4–5 | `requestId` | LE `uint16` |
| 6 | `speedPct` | 0–100 |
| 7–15 | `positionsPacked` | 6×12-bit STS positions (0–4095), slots 0..5 = IDs 1..6 |

**Calibration:** leader still runs `remapServoPositionWithCalibration` before encode. Wire carries **follower STS counts** (already remapped). No calibration bytes on the wire in phase 1.

### Tests (native Unity)

- `teleop_position_12bit_pack` round-trip
- Turbo codec encode/decode + mask/id mapping
- Legacy codec byte layout unchanged vs pre-refactor
- Remap → turbo encode → decode position match

## Phase 2 — Planned (not implemented)

| Item | Description | Benefit |
|------|-------------|---------|
| **B — Sparse + keyframe** | Bitmap + only moved axes; full keyframe every 100–150 ms or 10 frames | Fewer bytes when 1–2 joints move |
| **C — Per-joint range codes** | Map `[min,max]` from `CalibrationProfile` to N bits per joint | Smaller than 12 bits when range is narrow |
| **D — Delta on wire** | `int8` Δ after keyframe | Industrial CAN-style |

## Phase 3 — Optional

- Separate ESP-NOW peer message type registry (like Wi-Fi `WifiDirectOfferPacket` sizing).
- Keyframe carries `calibrationProfileHash` for mismatch detection (no runtime renegotiation yet).

## Flash checklist

Turbo compact changes **wire format** for turbo only. Flash **leader + follower** together when updating phase 1+.

```powershell
python ESP32/tools/check_structural_limits.py --project-root ESP32
cd ESP32
pio test -e native
pio run -e leader -e follower
```

## Acceptance criteria (phase 1)

- [ ] Classic ESP-NOW teleop unchanged on the wire (legacy codec).
- [ ] Turbo uses 16-byte packet; `sizeof` verified in unit tests.
- [ ] Round-trip position error ≤ 0 counts (12-bit exact for 0–4095).
- [ ] `pio test -e native` green.
- [ ] Bench: turbo visually smooth as classic; `dr` stays 0.

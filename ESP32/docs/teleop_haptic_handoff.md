# Teleop haptic feedback — handoff / resume guide

**Last updated:** 2026-06-02 (bench session)  
**Parent spec:** [teleop_espnow_feedback.md](teleop_espnow_feedback.md) (Phase 5.1 + 5.2)  
**Profile:** `ControllerOperationProfile::TeleopEspNowFeedback` (OLED menu leaf)

Use this file to resume haptic work on another machine **without** the full chat history.

---

## What we are building

Dual-board SO-101 teleop over ESP-NOW turbo downlink + **autonomous load uplink** (~80 Hz). Phase **5.1** shows per-joint loads on the leader OLED (`fb:xxHz`). Phase **5.2** adds **gripper-only haptic return** on the leader: when the follower gripper hits something, the operator feels resistance (and optionally position alignment) on the leader trigger.

**Design tension (core problem):**

- **Mirror path:** leader position → follower goals (continuous, ~12 ms).
- **Feedback path:** follower load + present position → leader torque/goal overlay.

These fight each other if haptic engages during **free-air closing** (mirror lag looks like “contact”). Most tuning so far is about **contact detection** vs **false positives at empty**.

---

## Current bench status (honest)

| Area | Status |
|------|--------|
| Load UL + OLED `fb` Hz | OK (~70–80 Hz) |
| Haptic on **real object** (firm grip) | **Good** — torque rises clearly on leader |
| Free-air teleop | **Much better** than early builds, but **occasional false haptic** still reported |
| Position sync on contact | Works when load is firm; gated to avoid mirror-lag jerks |
| Strict ping-pong / soft-wait `requestId` | **Not implemented** (deferred) |

**Open issue to fix next:** sporadic haptic kick at empty air — likely residual mirror lag + low-speed follower load spikes passing contact filters.

---

## Message flow (as implemented)

```text
Leader mirror task (~12 ms)     → ESP-NOW turbo DL → Follower apply (SyncWrite only)
Follower LoadSampler (~12 ms)    → syncRead pos+speed+load → ESP-NOW TeleopLoadFeedback UL
Leader main loop                 → pollTeleopLoadFeedback → applyTeleopHapticOverlay (~12 ms)
```

UL is **decoupled** from mirror apply (no piggyback). Both boards must be flashed together when wire layout changes.

---

## Wire format: `TeleopLoadFeedback` (13 bytes)

`PresenceMessageType::TeleopLoadFeedback = 13`, `wireVersion = 1` (bump only if layout changes; no product versioning).

| Offset | Field |
|--------|--------|
| 0 | magic `0xA5` |
| 1 | wireVersion `1` |
| 2 | messageType `13` |
| 3–4 | `requestId` uint16 LE (follower seq ≥ 50000) |
| 5–10 | `load[6]` uint8 — STS \|load\|/8, motion-filtered |
| 11–12 | `gripperPresentPos` uint16 LE — follower J6 STS present |

Codec: `src/common/teleop/teleop_load_feedback_codec.*`

---

## Haptic behaviour (gripper J6 only)

### Follower (load uplink)

1. `syncReadPresentLoad` reads block from `PRESENT_POSITION_L` (pos, speed, load).
2. Arm joints: zero load if `MOVING≠0` or \|speed\|>80.
3. Gripper: ignore `MOVING`; zero raw load if \|speed\|>220 (spike filter).
4. `netGripperLoadWire` subtracts **idle baseline** (baseline does **not** chase sustained contact load).
5. **Contact uplink filter:** if gripper \|speed\| > `kTeleopLoadGripperContactMaxAbsSpeed` (32), force gripper wire load to **0** before TX.

### Leader (engage / apply)

Engage only when **all** of:

- `gripperLoad ≥ kTeleopHapticPositionSyncMinWireLoad` (18) — firm contact only
- `|leaderPresent − followerOnLeader| ≥ kTeleopHapticMinPositionGap` (40 STS counts)
- `kTeleopHapticEngageStreakRequired` (2) consecutive 12 ms ticks (~24 ms debounce)

**Position goal** (`selectGripperHapticGoal`):

- Load **< 18:** goal = leader present (should not engage anyway)
- Load **≥ 18:** goal = follower present remapped to leader calibration

**Disengage:**

- Immediate if mirror caught up: gap < `kTeleopHapticMirrorCatchUpGap` (18) and load < 18
- Or load ≤ `kTeleopHapticGripperDisengageMaxWireLoad` (4) for `kTeleopHapticDisengageHoldMs` (120 ms)

**Torque:** `mapWireLoadToTorqueLimit(load, isGripper=true)` — gripper min 320, max 1000, gain ×2.5.

Logic module: `src/common/teleop/teleop_haptic_contact.*`  
Overlay tick: `src/leader/leader_app_teleop_haptic.cpp`

---

## Key source files

| Area | Files |
|------|--------|
| Wire codec | `src/common/teleop/teleop_load_feedback_codec.*` |
| Contact policy | `src/common/teleop/teleop_haptic_contact.*` |
| Load → torque | `src/common/teleop/teleop_haptic_mapper.*` |
| Leader ingest | `src/leader/leader_app_teleop_feedback.cpp`, `leader_presence_service_handlers.cpp` |
| Leader haptic | `src/leader/leader_app_teleop_haptic.cpp` |
| Follower sampler | `src/follower/follower_teleop_load_sampler_task.cpp` |
| STS sync read | `src/common/servo/servo_bus_service_sync_read.cpp` |
| Haptic bus write | `src/common/servo/servo_bus_service_haptic.cpp` |
| Leader tunables | `src/Config/leader_runtime_config.h` |
| Follower tunables | `src/Config/follower_runtime_config.h` |

**Removed (dead code):** `FollowerTeleopLoadSnapshot` — was write-only after autonomous UL; deleted.

---

## Tunables (current values)

### Leader — `leader_runtime_config.h`

| Constant | Value | Role |
|----------|-------|------|
| `kTeleopHapticPeriodMs` | 12 | Haptic overlay tick |
| `kTeleopHapticGripperEngageMinWireLoad` | 18 | Min load (engage = position sync threshold) |
| `kTeleopHapticPositionSyncMinWireLoad` | 18 | Apply follower position on leader |
| `kTeleopHapticMinPositionGap` | 40 | Min leader/follower gap to engage |
| `kTeleopHapticMirrorCatchUpGap` | 18 | Disengage when mirror caught up |
| `kTeleopHapticEngageStreakRequired` | 2 | Debounce ticks before engage |
| `kTeleopHapticGripperDisengageMaxWireLoad` | 4 | Disengage threshold |
| `kTeleopHapticDisengageHoldMs` | 120 | Disengage hold time |

### Leader — `teleop_haptic_mapper.h`

| Constant | Value |
|----------|-------|
| `kGripperTorqueLimitMin` | 320 |
| `kGripperTorqueLimitMax` | 1000 |
| `kGripperGainNumerator/Denominator` | 5/2 (×2.5) |

### Follower — `follower_runtime_config.h`

| Constant | Value | Role |
|----------|-------|------|
| `kTeleopLoadSamplerPeriodMs` | 12 | Sampler period |
| `kTeleopLoadGripperContactMaxAbsSpeed` | 32 | Zero gripper UL load if faster |
| `kTeleopLoadGripperMaxAbsSpeed` | 220 | Raw load spike zero in sync read |

---

## Ideas for next tuning (false positives at empty)

Try in this order (leader + follower flash unless noted):

1. **Raise** `kTeleopHapticMinPositionGap` (40 → 50–55) — mirror lag vs real block.
2. **Lower** `kTeleopLoadGripperContactMaxAbsSpeed` (32 → 24) — stricter “must be stalled” on follower.
3. **Raise** engage streak (2 → 3) — +12 ms latency, fewer glitches.
4. **Raise** firm load threshold (18 → 20) if light objects still work.
5. **Send gripper abs speed in UL** (14-byte wire) so leader can require speed < N at engage time (today speed filter is follower-only).
6. **Compare leader commanded gripper goal** (last mirror batch) vs follower present instead of leader present vs follower — separates operator pose from mirror lag.

Do **not** re-introduce “leader must be still to engage” without a bypass for firm contact — that caused ~1 s haptic delay in early builds.

---

## Build, test, flash

From repo root `SoArm101-WaveshareEsp32/`:

```bash
python ESP32/tools/check_structural_limits.py --project-root ESP32
cd ESP32
pio test -e native -f test_teleop_load_feedback_codec -f test_teleop_haptic_mapper -f test_teleop_haptic_contact
pio run -e leader -e follower
```

Flash **both** boards after haptic or wire changes.

---

## Native tests

| Test | Covers |
|------|--------|
| `test_teleop_load_feedback_codec` | Wire encode/decode, baseline |
| `test_teleop_haptic_mapper` | Load → torque mapping |
| `test_teleop_haptic_contact` | Engage gap, position sync, mirror catch-up |

---

## Evolution notes (what we tried)

1. **Load-only engage** → false positives + latency when leader moving.
2. **Double EMA** on load → ~1 s felt delay; removed; use raw last UL sample.
3. **Position feedback always** → jerks from follower lag; now gated to firm load only.
4. **Torque-only pre-contact** (load 10–17) → “crisp” feel at empty; removed; engage only at firm load (18+).
5. **Baseline EWMA chasing contact** → torque dropped to zero while still blocked; fixed in `netGripperLoadWire`.

---

## Related docs

- [teleop_espnow_feedback.md](teleop_espnow_feedback.md) — full Phase 5 architecture
- [oled_menu_refactor_plan.md](oled_menu_refactor_plan.md) — menu phases
- [CODE_CONTEXT_INDEX.md](CODE_CONTEXT_INDEX.md) — doc index

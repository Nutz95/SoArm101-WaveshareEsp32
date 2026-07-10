# OLED interactive menu — refactor plan

**Status:** menu phases 0–4 complete; **Phase 5.1** feedback teleop implemented (bench on hardware); **Phase 6** = IK teleop.  
**Goal:** replace the implicit “Mode cycles profiles” UX with a **navigable OLED menu** that scales as features grow.

**User guide:** [oled_menu.md](oled_menu.md) · **Feedback teleop (next):** [teleop_espnow_feedback.md](teleop_espnow_feedback.md)

## Problem today (original)

- `OledMenuController` only **routes** to a few static layouts (`showDashboard`, calibration screens, Wi-Fi / OTA prompts).
- **Profile selection** was tied to Xbox **Mode** cycling through `ControllerOperationProfile` enum order — hard to discover, no hierarchy, no scroll.
- **ESP-NOW pairing reset** was only available via dashboard / USB serial — not on the OLED.

**Resolved** for normal use: hierarchical menu, pairing reset, calibration submenu, OTA leaf, Mode no longer cycles profiles.

## Target UX (128×32, 4 text lines)

See [oled_menu.md](oled_menu.md) for layout, controls, and flows.

## Design pattern (as implemented)

**State pattern:** each screen implements `IOledMenuScreen` (`render` + `onInput`).  
**List screens:** `OledMenuListScreenBase` + scroll model (`OledListScrollModel`).  
**Navigator:** stack of `OledMenuScreenId` (push on enter, pop on **B** / **Back** item).

**Item actions:** constexpr lookup tables keyed by item enums (`OledMenuRootItem`, `OledMenuTeleopItem`, …) with designated initializers.

**Browse vs preview:** `oledMenuBrowseMode_` + `oledMenuResumeScreen_` — cancel returns to **parent** menu.

## Menu tree (v1)

```text
Root
├── Info                         ✅
├── Teleop                       ✅
│   ├── ESP-NOW                  ✅
│   ├── ESP-NOW Turbo            ✅
│   ├── Wi-Fi                    ✅
│   ├── IK Teleop                ✅ stub
│   ├── ESP-NOW Feedback         ⏳ Phase 5
│   └── Back                     ✅
├── Passthrough                  ✅
├── Calibration                  ✅
├── Pairing (Status / Reset)     ✅
└── OTA                          ✅
```

## Phased delivery

### Phase 0 — Design sign-off ✅

### Phase 1 — Framework + Info + Pairing read-only ✅

### Phase 2 — Teleop + Passthrough ✅

### Phase 3 — Calibration + Pairing reset ✅

- [ ] Hardware test: reset from OLED → follower re-pairs (manual)

### Phase 4 — OTA + deprecate profile-cycle UX ✅

- [x] OTA leaf from root menu
- [x] `handleModeCycleProfileStep` removed — profiles chosen only from OLED menu
- [x] User documentation: [oled_menu.md](oled_menu.md) + README pointer

### Phase 5 — ESP-NOW feedback teleop ⏳ **next**

Extends **turbo** downlink with compact **load uplink** (6 servos; gripper ID 6 priority on OLED).

**Architecture (signed off in doc):** **async piggyback** — follower sends `TeleopLoadFeedback` after each apply (`write` → `read load` → `UL`); leader mirror stays **~12 ms periodic** (does not block on UL). OLED shows loads + **`fb:xxHz`**. Strict ping-pong reserved for optional 5.2 haptic tuning.

| Sub-phase | Deliverable |
|-----------|-------------|
| **5.1** | Menu leaf, codec, follower load `SyncRead`, piggyback UL, OLED + **fb Hz** |
| **5.2** | Leader torque overlay, optional soft-wait, quasi–zero-G (bench) |

Full spec + mermaid bus timelines: **[teleop_espnow_feedback.md](teleop_espnow_feedback.md)**

### Phase 6 — IK teleop

- [ ] Enable IK leaf; sticks/triggers for arm control
- [ ] Menu navigator idle while IK active

## Integration points

| Concern | Now |
|---------|-----|
| Input (menu) | `handleInteractiveOledMenuInput` → `OledMenuNavigator::onInput` |
| Input (teleop/cal/OTA) | `handleControllerModeCycleEvents` — **A** / **B** only |
| Mode button | Root menu: highlight down only (discarded during profile preview) |
| Pairing reset | OLED **Pairing → Reset** |
| Draw (menu) | `refreshInteractiveOledMenu` |

## Testing strategy

1. **Native:** `pio test -e native -f test_oled_menu_navigator` (13 tests).
2. **Hardware:** [teleop_radio_fluency.md](teleop_radio_fluency.md).
3. **Feedback (phase 5):** codec unit tests + bench `dr` / OLED latency.

---

*Linked from [CODE_CONTEXT_INDEX.md](CODE_CONTEXT_INDEX.md).*

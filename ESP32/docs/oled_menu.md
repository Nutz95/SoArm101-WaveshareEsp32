# Leader OLED interactive menu

The leader board shows a **navigable menu** on the 128×32 SSD1306 display at boot and whenever you are not in an active teleop, calibration, OTA, or passthrough session.

**Controller reference:** [Xbox button diagram](assets/XBoxControler.jpg) (same image as in [calibration.md](calibration.md)).

## Layout

The display has **four text lines** (small font, ~22 characters per line). List screens show a **cursor** `>` on the selected row. Only four rows are visible at once; the list **scrolls** so the cursor always stays on screen.

Example at boot (root menu):

```text
> Info
  Teleop
  Passthrough
  Calibration
```

When you move down, the window scrolls:

```text
  Teleop
> Passthrough
  Calibration
  Pairing
```

Detail screens (Info, pairing status, IK stub, reset confirm) fill fixed lines instead of a list.

## Xbox controls (menu browse)

| Button | Role in menu |
|--------|----------------|
| **D-pad up / down** | Move highlight up or down (wraps at ends) |
| **View (Mode)** | At **root only:** move highlight **down** (same as D-pad down) |
| **A** | Enter submenu or activate the selected item |
| **B** | Back to parent screen (`Back` row does the same) |

**Sticks, bumpers, triggers** are not used for navigation (reserved for future IK teleop).

## Menu tree

```text
Root
├── Info              → read-only network / link summary
├── Teleop
│   ├── ESP-NOW       → mirror profile (legacy batch)
│   ├── ESP-NOW Turbo → compact sparse frames + latency OLED
│   ├── ESP-NOW Feedback → turbo downlink + load uplink OLED (fb Hz)
│   ├── Wi-Fi         → soft-AP direct teleop
│   ├── IK Teleop     → stub (“not implemented”)
│   └── Back
├── Passthrough       → USB serial ↔ leader servo bus
├── Calibration
│   ├── Leader
│   ├── Follower
│   └── Back
├── Pairing
│   ├── Status        → paired / MAC / link hint
│   ├── Reset         → confirm with A, then clears ESP-NOW pairing
│   └── Back
└── OTA               → Wi-Fi firmware update (needs LAN)
```

## Typical flows

### Start ESP-NOW teleop

1. Boot → root menu appears.
2. **Teleop** → **A** → **ESP-NOW** (or Turbo / Wi-Fi) → **A**.
3. OLED shows the teleop preview (“teleop mirror start?” / press **A**).
4. **A** → mirroring runs; **B** → stop and return to the **Teleop** submenu.

### Calibration

1. **Calibration** → **Leader** or **Follower** → **A**.
2. Preview: **A** to start, **B** to skip → back to **Calibration** list.
3. During capture: existing calibration workflow on OLED; **B** cancels → **Calibration** list.

### OTA

1. **OTA** → **A** on preview → STA connects; flash with `pio run -e leader-ota -t upload` or `build_upload_leader.ps1 --ota`.
2. **B** skips or ends OTA → back to root menu.

### Reset pairing

1. **Pairing** → **Reset** → **A** on confirm screen.
2. Leader clears NVS pairing and notifies the follower; both arms can re-pair automatically when powered nearby.

## When the menu is hidden

The interactive menu is **suspended** while:

- Teleop mirror is active (after **A** on preview)
- Calibration is engaged
- OTA preview / active OTA
- Passthrough engaged
- Wi-Fi teleop link setup (profile `TeleopWifi` preview)

When you cancel with **B**, the menu reopens at the **parent** screen you came from (e.g. Teleop list, not always root).

## Turbo teleop OLED (not the menu)

During **ESP-NOW Turbo** mirror, the display shows **latency / drop** metrics (`lat`, `dr`) instead of the menu. Classic ESP-NOW uses the legacy status layout.

**Planned:** [ESP-NOW feedback teleop](teleop_espnow_feedback.md) — torque display on OLED, then haptic return on the leader arm.

## Related docs

- Implementation plan: [oled_menu_refactor_plan.md](oled_menu_refactor_plan.md)
- Xbox profiles (legacy reference): [architecture/xbox_ble_controls.md](architecture/xbox_ble_controls.md)
- Calibration workflow: [calibration.md](calibration.md)
- Radio / fluency when switching modes: [teleop_radio_fluency.md](teleop_radio_fluency.md)

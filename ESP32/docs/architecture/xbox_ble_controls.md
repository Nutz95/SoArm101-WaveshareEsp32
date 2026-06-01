# Xbox BLE controller (leader)

Leader builds with `LEADER_ENABLE_XBOX_BLE=1` (`env:leader`, `env:leader-ota`, `build_upload_leader.ps1`).

## Radio / boot

1. After NVS: **NimBLE init** (`[BOOT] stage: xbox BLE`).
2. Then Wi-Fi STA + ESP-NOW.
3. With BLE active, firmware sets **`WiFi.setSleep(true)`** (ESP-IDF requirement for Wi-Fi + Bluetooth coexistence).
4. Do **not** call `esp_bt_controller_mem_release()` outside NimBLE (NimBLE-Arduino already releases classic BT once).

## Profile cycle (Mode button)

Order when cycling with the Xbox **Mode** button (or dashboard `xbox_mode_cycle_button_set`):

| Step | Profile | OLED / status hint |
| ---- | ------- | ------------------ |
| 0 | CalibrationLeader | Preview only: `cal leader? press A` |
| 1 | CalibrationFollower | Preview only: `cal follower? press A` |
| 2 | TeleopEspNow | ESP-NOW teleop (Wi-Fi `:9090` stream paused) |
| 3 | TeleopWifi | UDP teleop via router |
| 4 | Passthrough | USB servo bus passthrough — **A** = engage |
| 5 | TeleopPcSerial | PC COM mirror (legacy) |
| 6 | **OtaReady** | STA forced on — flash OTA from PC |

Important distinction:

- Cycling selects a profile preview.
- Calibration does **not** start on profile selection.
- Calibration starts only when the user confirms with **A** while a calibration profile is selected.

## Calibration buttons (OLED, Xbox flow)

| Button | Calibration preview (not started) | Phase 0 (arm prompt) | Phase 1 (min/max table) |
| ------ | --------------------------------- | --------------------- | -------------------------- |
| **A** | Enter selected calibration mode | Apply center (leader: local offsets; follower: ESP-NOW center) | Validate & save calibration |
| **B** | Skip calibration (return TeleopEspNow) | Cancel (exit cal profile) | Cancel |

OLED flow when using Xbox profile cycle:

1. **Preview** — `Not started`, `Enter? (A)`; no calibration state changes yet.
2. **On A** — calibration engages and starts a 3.5 s safety re-arm (`Wait 3s ... Wait 1s`).
3. **Arm prompt** — `Place center`, then `A:Center B:Can` once re-arm delay has elapsed.
4. **Centering…** — follower only, while ESP-NOW center runs (leader centers immediately on **A**).
5. **Min/max table** — only after center finished; move joints, then `A:Val B:Can`.

There is **no** auto-advance timer from arm position; phase 1 opens only after **A** and center complete.

Dashboard can still start calibration directly via `teleop_transport_set` to calibration profiles. Once calibration is engaged, `teleop_calibration_capture` values are unchanged: `2` = center, `3` = finish, `4` = cancel.

## Dashboard commands (Wi-Fi or USB)

Same binary command frames as TCP `:9090` (magic `0x5343`). See `tools/telemetry_dashboard/dashboard_protocol.py`.

Notable command IDs:

- `17` — `teleop_transport_set` (profile / cal mode)
- `18` — `xbox_mode_cycle_button_set` (virtual Mode press)
- `19` — `teleop_calibration_capture` (center / finish / cancel)

## USB debug (Phase 3)

When Wi-Fi telemetry is paused (e.g. TeleopEspNow), connect the dashboard to the leader **USB COM** port at `USB_CDC_BAUD` (115200 default):

```powershell
cd ESP32/tools/telemetry_dashboard
python telemetry_dashboard.py --leader-serial COM7 --leader-serial-baud 115200
```

Protocol: identical command + snapshot packets as Wi-Fi stream (`START_STREAM` / `STOP_STREAM`).

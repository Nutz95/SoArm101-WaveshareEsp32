# Xbox BLE controller (leader)

Leader builds with `LEADER_ENABLE_XBOX_BLE=1` (`env:leader`, `env:leader-ota`, `build_upload_leader.ps1`).

## Pairing and stored address

On **every leader boot**:

1. Any previously stored Xbox BLE address is **cleared from NVS**.
2. Firmware runs a **full active BLE scan** (4 s window) to find a HID / Xbox controller.
3. After a successful connect, the controller **BLE address is saved to NVS** (`xbox_ble` / `ctrl_mac`) and kept in RAM for the rest of the session.

While the leader keeps running (no reboot):

- If the pad disconnects or sleeps, reconnect uses the **stored address first** (direct connect, no scan).
- During **active teleop mirror**, reconnect retries are **address-only** (no BLE scan) to avoid radio glitches on ESP-NOW / Wi-Fi teleop.

**Pair a different Xbox controller:** power-cycle or reboot the leader so boot clears the old address and runs a full scan again. Mid-session pairing of a new pad is not supported.

## Radio / boot

1. After NVS: **NimBLE init** (`[BOOT] stage: xbox BLE`).
2. Then Wi-Fi STA + ESP-NOW.
3. With BLE active, firmware sets **`WiFi.setSleep(true)`** (ESP-IDF requirement for Wi-Fi + Bluetooth coexistence).
4. Do **not** call `esp_bt_controller_mem_release()` outside NimBLE (NimBLE-Arduino already releases classic BT once).

## OLED menu (profiles and navigation)

The **View** button is **button 6** on the controller diagram (two small squares left of the Xbox logo). Firmware calls it **Mode**.

![Xbox Wireless Controller — button indices](../assets/XBoxControler.jpg)

**At boot** the leader shows the **interactive OLED menu**. Pick ESP-NOW, turbo, Wi-Fi, calibration, passthrough, pairing, or OTA from the tree — **Mode no longer cycles profiles**.

| Control | Role |
|---------|------|
| **D-pad up/down** | Move menu cursor |
| **Mode (View)** | At **root menu only:** move highlight down |
| **A** | Enter submenu / confirm leaf / start teleop or calibration |
| **B** | Back to parent menu / cancel preview or active session |

Full user guide: [oled_menu.md](../oled_menu.md).

**Sticks, bumpers, triggers** — reserved for future **IK teleop**.

### Profile preview (after menu leaf)

| Profile | OLED / status hint |
| ------- | ------------------ |
| CalibrationLeader | `cal leader? press A` |
| CalibrationFollower | `cal follower? press A` |
| TeleopEspNow | ESP-NOW teleop — **A** starts mirror |
| TeleopEspNowTurbo | Turbo + `lat` / `dr` on OLED |
| TeleopWifi | Wi-Fi Direct — **A** = link, **A** = mirror |
| Passthrough | **A** = engage USB passthrough |
| OtaReady | **A** = STA for OTA flash |

Dashboard `xbox_mode_cycle_button_set` (command `18`) is **deprecated** for profile selection; use `teleop_transport_set` or the OLED menu flow.

## Calibration buttons (OLED, Xbox flow)

| Button | Calibration preview (not started) | Phase 0 (arm prompt) | Phase 1 (min/max table) |
| ------ | --------------------------------- | --------------------- | -------------------------- |
| **A** | Enter selected calibration mode | Apply center (leader: local offsets; follower: ESP-NOW center) | Validate & save calibration |
| **B** | Skip calibration (return TeleopEspNow) | Cancel (exit cal profile) | Cancel |

OLED flow when calibration is started from the **menu** (or dashboard `teleop_transport_set`):

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

When Wi-Fi telemetry is paused (e.g. TeleopEspNow), use the leader **USB COM** port (same binary protocol as `:9090`).

Salon default (reads `monitor_port` / baud from `ESP32/platformio.ini`):

```powershell
cd ESP32/tools/telemetry_dashboard
.\start_dashboard.ps1
```

Manual override:

```powershell
python telemetry_dashboard.py --leader-serial COM7 --leader-serial-baud 115200
```

Protocol: identical command + snapshot packets as Wi-Fi stream (`START_STREAM` / `STOP_STREAM`). Text boot logs on USB are suppressed while the stream is active so binary frames stay clean.

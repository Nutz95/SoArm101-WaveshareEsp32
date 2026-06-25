# Calibration (NVS)

Joint **min / max** ranges are stored in **flash (NVS)** on each ESP32 so teleop can map leader joystick motion to follower-safe goals without a PC config file — aligned with the LeRobot idea of per-arm calibration, but **on-device**.

## What is stored

`CalibrationProfile` (per arm, 6 servos):

| Field | Meaning |
|-------|---------|
| `minPosition[i]` | Lowest STS present position seen for servo ID `i+1` |
| `maxPosition[i]` | Highest STS present position seen for servo ID `i+1` |

Default if missing: `0 … 4095` (full STS range) — written automatically on first boot.

Storage namespace: NVS `soarm-cal`, keys `leader` and `follower` (`NvsCalibrationStore`).

## Where profiles live

| Board | NVS key | Used for |
|-------|---------|----------|
| **Leader ESP32** | `leader` | Local leader arm remap + leader calibration workflow |
| **Leader ESP32** | `follower` | **Mirror remap** leader → follower joint space during teleop |
| **Follower ESP32** | `follower` | Local follower execution / follower-side calibration commit |

The **leader always keeps a copy of the follower profile in RAM/NVS** because mirroring applies:

```text
leader raw position → leader profile → follower profile → follower goal
```

(`remapServoPositionWithCalibration` in the mirror task.)

When you calibrate the **follower** from the leader (Xbox cal follower profile), the leader updates and saves **both** its cached follower profile and sends commands so the follower arm moves and stores its own `follower` key when appropriate.

## How to calibrate (with Xbox controller)

1. Cycle **Mode** button to **Calibration leader** or **Calibration follower** (preview only).
2. Press **A** to start (not automatic on profile select).
3. **Center** — first **A** in phase 0 (follower: wait for center move over ESP-NOW).
4. Move joints through **min and max**; OLED shows captured table.
5. Second **A** validates and **writes NVS**; **B** cancels.

Details: [architecture/xbox_ble_controls.md](architecture/xbox_ble_controls.md).

Dashboard commands (`teleop_calibration_capture`, profile set) can also drive calibration without Xbox.

## After calibration

- Ranges persist across **reboot** and **OTA** (same NVS partition unless erased).
- Teleop smoothness depends on sensible min/max — if ranges are still default full-scale, mapping is linear but may not match your mechanical limits.
- Re-calibrate one arm without re-flashing the other board.

## Code references

| Component | Path |
|-----------|------|
| Profile struct | `src/common/types/calibration_profile.h` |
| NVS load/save | `src/common/nvs_calibration_store.cpp` |
| Remap math | `src/common/calibration/calibration_profile_utils.cpp` |
| Leader workflow | `src/leader/leader_app_commands_calibration.cpp`, `leader_calibration_workflow.cpp` |
| Follower save | `src/follower/follower_app.cpp` |

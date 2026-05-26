# Refactor Checklist

This checklist tracks the current multi-phase implementation plan for dashboard UX, controller pairing, calibration, and follower control architecture.

## Phase 1 - Immediate UX + Readability

- [x] 1.1: Show full operation mode on OLED line 3 for 128x32 screens (`M: CALIB`, `M: TELEOP ESPNOW`, `M: TELEOP WIFI`).
- [x] 1.2: Remove 5th-line specific OLED rendering path that is not usable on 128x32 hardware.
- [x] 1.3: Rename short teleop transport variables (`speedPct`) to explicit names (`speedPercent`) in Wi-Fi packet and bridge code.
- [x] 1.4: Split dashboard HTML into composable view fragments and load them at startup.
- [ ] 1.5: Add dashboard smoke validation for fragment loading failure paths.

## Phase 2 - Operator Testability Controls

- [x] 2.1: Add "Center all servos" action in teleoperation view.
- [x] 2.2: Add per-servo "Center" action in manual commands view.
- [x] 2.3: Ensure center commands are safe with partial setup (for bench testing with only two servos connected).
- [x] 2.4: Add clear command status feedback for each center action.

## Phase 3 - Xbox Pairing and Mapping Wizard

- [ ] 3.1: Implement SoArm Bluetooth pairing flow using NimBLE callbacks (inspired by the callback model used in AutoBalancingAIBot).
- [ ] 3.2: Add dashboard wizard to guide pairing, button mapping, and validation.
- [ ] 3.3: Persist Xbox mapping/profile in NVS on the leader board.
- [ ] 3.4: Add button 6 cyclic mode switch behavior and expose active mode in telemetry.

## Phase 4 - Calibration Workflow on ESP (NVS)

- [ ] 4.1: Implement midpoint-based calibration wizard on-device (current target behavior).
- [ ] 4.2: Store calibration data in NVS on each board (leader and follower independently).
- [ ] 4.3: Reuse Lerobot calibration concepts (homing offset + captured range) while keeping firmware-native storage format.
- [ ] 4.4: Defer multi-turn overlap handling and advanced edge cases to a later phase.

## Phase 5 - Teleop Robustness and Regression Coverage

- [ ] 5.1: Add non-regression tests for live apply behavior (checkbox/select/blur/change) to prevent rollback regressions.
- [ ] 5.2: Add tests for continuous speed persistence (prevent reset to 35).
- [ ] 5.3: Add transport switch latency comparison checks (ESP-NOW vs Wi-Fi).

## Phase 6 - Follower-Only Pose Control (Inverse Kinematics)

- [ ] 6.1: Add new mode where the leader computes follower joint targets from desired end-effector pose.
- [ ] 6.2: Keep leader servos passive in this mode (leader used as compute/control source only).
- [ ] 6.3: Send computed joint targets to follower only, with existing safety guards.
- [ ] 6.4: Add telemetry to expose requested pose, solved joints, and solver validity.

## Validation Gates

- [ ] V.1: `python tools/check_structural_limits.py --project-root .`
- [ ] V.2: `pio run`
- [ ] V.3: `pio test -e native`
- [ ] V.4: Dashboard smoke test (`tools/telemetry_dashboard/start_dashboard.ps1`)
- [ ] V.5: Field test: transport switch + continuous mirror without rollback

## Notes

- Keep all committed code and documentation in English.
- Keep this checklist updated at every phase completion.

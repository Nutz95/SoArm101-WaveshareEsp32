# Code Context Index

This document provides a fast entry point to the most important modules.
Use it when implementing changes to avoid broad code searches.

## Maintenance Rule

- Update this index on every PR that adds/removes key files under `src/` or `tools/telemetry_dashboard/`.
- Update this index when a file or class responsibility changes materially, even if file paths stay the same.
- If key runtime/transport/teleop behavior changed, update both:
  - this file (`docs/CODE_CONTEXT_INDEX.md`)
  - refactor tracking file (`docs/REFACTORING_PLAN.md`)

## Link heartbeat (Phase 1)

- `src/common/link/link_heartbeat_manager.h` — peer liveness, heartbeat/full-presence TX cadence.
- `src/common/link/link_constants.h` — `kHeartbeatIntervalMs`, `kFullPresenceIntervalMs`, `kPeerAliveTimeoutMs`.
- `src/common/presence/link_heartbeat_packet.h` — compact ESP-NOW keepalive + staged ACK fields.

- `18` — `xbox_mode_cycle_button_set` (legacy; no longer cycles profiles on leader)

## Leader OLED menu (firmware)

- `docs/oled_menu.md` — user guide (layout, controls, flows).
- `docs/oled_menu_refactor_plan.md` — implementation phases.
- `src/leader/oled_menu/` — `OledMenuNavigator`, list/detail screens, item action tables.
- `src/leader/leader_app_oled_menu.cpp` — browse mode, profile/pairing actions, menu input.

## ESP-NOW feedback teleop (planned)

- `docs/teleop_espnow_feedback.md` — turbo + load uplink, OLED torque display, leader haptic.
- `docs/teleop_haptic_handoff.md` — Phase 5.2 haptic resume guide (bench status, tunables, open issues).

## Leader Firmware

- `src/leader/leader_app.cpp`
  - Main orchestration loop, snapshot production, task startup.
- `src/leader/leader_app_mode.cpp`
  - Operation-mode selection, follower state synthesis, and status LED rendering for normal and calibration phases.
- `src/leader/leader_app_tasks.cpp`
  - Background task creation and task entrypoints.
- `src/leader/leader_app_ack_tracking.cpp`
  - Command tracking, retry scheduling, and transient status updates.
- `src/leader/leader_app_commands.cpp`
  - Dashboard command routing and command execution.
- `src/leader/leader_app_commands_teleop_continuous.cpp`
  - Continuous teleoperation command parsing, speed update, and enable/disable handling.
- `src/leader/leader_app_commands_teleop_transport.cpp`
  - Runtime teleop transport mode command handling (ESP-NOW/Wi-Fi UDP) and dashboard-initiated calibration engagement.
- `src/leader/leader_app_commands_calibration.cpp`
  - Leader-side teleop calibration capture commands (center/finish/cancel/sample), local NVS persistence, and follower capture forwarding.
- `src/leader/leader_app_controller_profile.cpp`
  - Xbox **A** / **B** handling for active profiles (teleop, cal, OTA); profile changes via OLED menu.
- `src/leader/leader_calibration_workflow.cpp`
  - Pure calibration parsing helpers extracted for host-side unit tests.
- `src/leader/leader_presence_service.cpp`
  - ESP-NOW pairing, frame ingest, and follower state updates.
- `src/leader/leader_presence_transport.cpp`
  - ESP-NOW outbound frame builders/senders for pairing and servo control.
- `src/leader/leader_servo_telemetry_task.cpp`
  - Background servo telemetry refresh and scan gating.
- `src/leader/leader_teleop_mirror_task.cpp`
  - Continuous teleop mirroring logic, batch sends, and send-to-ACK latency metrics.
- `src/leader/leader_teleop_mirror_task_internal.cpp`
  - Teleop mirror parsing, pending-batch bookkeeping, and latency/ACK helper logic extracted from the main loop file.
- `src/leader/leader_teleop_wifi_bridge.cpp`
  - Leader-side Wi-Fi UDP teleop batch sender and ACK polling.
- `src/leader/leader_xbox_controller_service.cpp`
  - NimBLE Xbox scan/connect/subscribe runtime service, telemetry snapshot, and logical button edge events.
- `src/leader/leader_xbox_controller_input.cpp`
  - Table-driven HID report decoding for buttons/D-pad/axes with controller-specific hat and analog normalization.
- `src/leader/leader_usb_debug_service.cpp`
  - Phase 3 USB CDC dashboard commands + telemetry snapshots (same protocol as Wi-Fi `:9090`).
- `src/common/presence/presence_message_type_name.cpp`
  - Lookup table for `PresenceMessageType` log names (includes `LinkHeartbeat`).

## Follower Firmware

- `src/follower/follower_app.cpp`
  - Main loop, incoming command execution, telemetry publication.
- `src/follower/follower_presence_service.cpp`
  - ESP-NOW receive/dispatch and control queue bookkeeping.
- `src/follower/follower_presence_transport.cpp`
  - ESP-NOW outbound presence, pair request, and ACK emission.
- `src/follower/follower_presence_queue.cpp`
  - Deduplicated servo-control queue enqueue/dequeue helpers.
- `src/follower/follower_teleop_wifi_bridge.cpp`
  - Follower-side Wi-Fi UDP teleop batch receiver and ACK sender.

## Shared Firmware Services

- `src/common/servo/servo_bus_service.cpp`
  - Servo scan/read/write operations and SyncWrite batch move.
- `src/common/servo/servo_bus_service_calibration.cpp`
  - Current-position-as-center calibration for detected STS servos with post-write verification.
- `src/common/servo/servo_bus_service_torque.cpp`
  - Servo torque enable/release operations extracted from ServoBusService core file.
- `src/common/servo/servo_bus_service_temperature.cpp`
  - Slow temperature polling and alarm hysteresis logic.
- `src/common/servo/servo_bus_service_refresh.cpp`
  - Fast known-servo telemetry refresh using cached ID list.
- `src/common/presence/presence_packet.h`
  - Presence frame binary layout.
- `src/common/presence/presence_message_type.h`
  - Presence message type IDs.
- `src/common/servo/servo_control_opcode.h`
  - Shared servo opcode IDs.
- `src/common/calibration/calibration_profile_utils.cpp`
  - Shared calibration profile capture from live telemetry and leader-to-follower position remapping helpers.
- `src/common/teleop/teleop_transport_mode.h`
  - Teleop transport mode enum shared by firmware modules.
- `src/common/teleop/teleop_wifi_packet.h`
  - Wi-Fi UDP teleop batch/ACK packet layouts and constants.

## Runtime Configuration

- `src/Config/common_runtime_config.h`
  - Cross-role constants.
- `src/Config/leader_runtime_config.h`
  - Leader timing, retry, and teleop runtime constants.
- `src/Config/follower_runtime_config.h`
  - Follower timing and runtime constants.

## Dashboard Bridge (Python)

- `tools/telemetry_dashboard/telemetry_client.py`
  - TCP telemetry stream client and command forwarding.
- `tools/telemetry_dashboard/telemetry_serial_client.py`
  - USB serial telemetry client (Phase 3, same protocol as TCP).
- `tools/telemetry_dashboard/dashboard_server.py`
  - HTTP API and static file serving.
- `tools/telemetry_dashboard/teleop_runtime.py`
  - Teleop card model, mapping logic, packed mirror values, and firmware latency fields.
- `tools/telemetry_dashboard/dashboard_protocol.py`
  - Binary protocol constants and stream snapshot struct layout.

## Dashboard UI (Web)

- `tools/telemetry_dashboard/static/index.html`
  - Dashboard structure, top-level tabs, and panel layout.
- `tools/telemetry_dashboard/static/views/pairing/pairing_control.html`
  - ESP pairing-only panel and pairing reset actions.
- `tools/telemetry_dashboard/static/views/pairing/xbox_pairing.html`
  - Dedicated Xbox runtime/configuration panel separated from ESP pairing.
- `tools/telemetry_dashboard/static/views/calibration/workflow.html`
  - Calibration workflow panel, isolated scan controls, and live calibration table.
- `tools/telemetry_dashboard/static/js/teleop_ui.js`
  - Teleop card rendering, position chart, and lag/tracking-error estimation.
- `tools/telemetry_dashboard/static/js/lag_metrics.js`
  - Lag search and tracking error statistics computed from leader/follower histories.
- `tools/telemetry_dashboard/static/js/dashboard_render.js`
  - Overview rendering, including temperature alarm indicators and calibration table state colors.
- `tools/telemetry_dashboard/static/js/calibration_render.js`
  - Calibration instruction text and table color-state rendering helpers.
- `tools/telemetry_dashboard/static/js/views/pairing_view.js`
  - Pairing and Xbox panel actions, controller-config persistence, and profile switching.
- `tools/telemetry_dashboard/static/js/views/calibration_view.js`
  - Calibration-panel actions, isolated scan handlers, and workflow status messaging.
- `tools/telemetry_dashboard/static/js/views/teleop_view.js`
  - Teleop actions and event handlers.
- `tools/telemetry_dashboard/static/css/teleop.css`
  - Teleop styles.

## Quality Gates

- `tools/check_structural_limits.py`
  - File size, function count, and class count guard.
- `test/test_structural_limits/test_main.cpp`
  - Structural guard execution in test suite.
- `test/test_teleop_runtime/test_main.cpp`
  - Teleop non-regression wrapper executing Python teleop runtime tests.
- `tools/telemetry_dashboard/tests/test_teleop_runtime.py`
  - Teleop runtime non-regression checks (mirror batch/start-stop/ID stability semantics).

## Typical Change Paths

- Teleop behavior update:
  - `src/leader/leader_teleop_mirror_task.cpp`
  - `src/follower/follower_app.cpp`
  - `src/follower/follower_presence_service.cpp`
  - `tools/telemetry_dashboard/teleop_runtime.py`
  - `tools/telemetry_dashboard/static/js/views/teleop_view.js`

- Pairing/transport update:
  - `src/leader/leader_presence_service.cpp`
  - `src/follower/follower_presence_service.cpp`
  - `src/common/presence/presence_packet.h`
  - `src/common/presence/presence_message_type.h`

- Telemetry field update:
  - `src/leader/leader_telemetry_snapshot.h`
  - `src/leader/leader_app.cpp`
  - `tools/telemetry_dashboard/dashboard_protocol.py`
  - `tools/telemetry_dashboard/telemetry_client.py`
  - `tools/telemetry_dashboard/dashboard_state.py`

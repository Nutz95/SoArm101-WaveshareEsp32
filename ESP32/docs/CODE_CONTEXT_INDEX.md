# Code Context Index

This document provides a fast entry point to the most important modules.
Use it when implementing changes to avoid broad code searches.

## Maintenance Rule

- Update this index on every PR that adds/removes key files under `src/` or `tools/telemetry_dashboard/`.
- Update this index when a file or class responsibility changes materially, even if file paths stay the same.
- If key runtime/transport/teleop behavior changed, update both:
  - this file (`docs/CODE_CONTEXT_INDEX.md`)
  - refactor tracking file (`docs/REFACTOR_CHECKLIST.md`)

## Leader Firmware

- `src/leader/leader_app.cpp`
  - Main orchestration loop, snapshot production, task startup.
- `src/leader/leader_app_tasks.cpp`
  - Background task creation and task entrypoints.
- `src/leader/leader_app_ack_tracking.cpp`
  - Command tracking, retry scheduling, and transient status updates.
- `src/leader/leader_app_commands.cpp`
  - Dashboard command routing and command execution.
- `src/leader/leader_presence_service.cpp`
  - ESP-NOW pairing, frame ingest, and follower state updates.
- `src/leader/leader_presence_transport.cpp`
  - ESP-NOW outbound frame builders/senders for pairing and servo control.
- `src/leader/leader_servo_telemetry_task.cpp`
  - Background servo telemetry refresh and scan gating.
- `src/leader/leader_teleop_mirror_task.cpp`
  - Continuous teleop mirroring logic and batch sends.

## Follower Firmware

- `src/follower/follower_app.cpp`
  - Main loop, incoming command execution, telemetry publication.
- `src/follower/follower_presence_service.cpp`
  - ESP-NOW receive/dispatch and control queue bookkeeping.
- `src/follower/follower_presence_transport.cpp`
  - ESP-NOW outbound presence, pair request, and ACK emission.
- `src/follower/follower_presence_queue.cpp`
  - Deduplicated servo-control queue enqueue/dequeue helpers.

## Shared Firmware Services

- `src/common/servo/servo_bus_service.cpp`
  - Servo scan/read/write operations and SyncWrite batch move.
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
- `tools/telemetry_dashboard/dashboard_server.py`
  - HTTP API and static file serving.
- `tools/telemetry_dashboard/teleop_runtime.py`
  - Teleop card model, mapping logic, packed mirror values.
- `tools/telemetry_dashboard/dashboard_protocol.py`
  - Binary protocol constants and struct layout.

## Dashboard UI (Web)

- `tools/telemetry_dashboard/static/index.html`
  - Dashboard structure and teleop panel layout.
- `tools/telemetry_dashboard/static/js/teleop_ui.js`
  - Teleop card rendering and position chart.
- `tools/telemetry_dashboard/static/js/dashboard_render.js`
  - Overview rendering, including temperature alarm indicators.
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
# runSubagent

Minimal context for subagents.

## Mission

- Keep changes small, testable, and reversible.
- Prefer edits in existing architecture paths.
- Avoid unrelated refactors.

## Required Reads

- README.md
- AGENTS.md
- CODING_RULES.md
- ESP32/docs/architecture/README.md

## Message Flow (summary)

- Dashboard -> Leader command socket with request_id.
- Leader applies local action and may forward over ESP-NOW to Follower.
- Follower sends ACK + telemetry in presence frame.
- Leader exposes merged status in telemetry snapshot.

## Validation

- Firmware build: `pio run -e leader -e follower-ota`
- Native tests: `pio test -e native`
- Dashboard syntax: `python -m py_compile telemetry_dashboard.py telemetry_client.py dashboard_protocol.py dashboard_server.py dashboard_state.py`

## Detailed References

- ESP32/docs/architecture/README.md
- ESP32/docs/architecture/plan.md

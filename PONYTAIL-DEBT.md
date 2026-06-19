# Ponytail debt ledger

Deliberate shortcuts (`ponytail:` comments) and tracked cleanup items.

## Active markers

| Location | What | Ceiling | Upgrade trigger |
| -------- | ---- | ------- | ---------------- |
| `leader_xbox_controller_service.cpp` | Teleop BLE reconnect uses direct address only (no scan) | Pad must wake and accept connect; no discovery of a new pad mid-session | User reports pad never reconnects during teleop after wake |
| `leader_xbox_controller_connect.cpp` | Xbox BLE address in NVS (`xbox_ble` / `ctrl_mac`) | Cleared every boot; no OS-level bonding | Persist across boot without full scan if salon needs faster pad attach |

## Migration backlog

| Tag | Item | Replacement | Scope |
| --- | ---- | ----------- | ----- |
| `stdlib` | `strncpy` in firmware | `copyCString()` from `ESP32/src/common/cstring_copy.h` | ~20 files under `ESP32/src/` (leader, follower, common) |

Last scan: migrate on next touch of each file; do not add new `strncpy`.

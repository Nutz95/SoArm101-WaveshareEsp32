# Communication refactor — action plan

Track progress here (no OpenSpec). Update this file and linked architecture docs as each phase lands.

**Goals**

- Coherent radio usage: ESP-NOW teleop without Wi-Fi stack contention (OTA only).
- Wi-Fi teleop via direct ESP↔ESP link (AP/STA + credentials over ESP-NOW), not dashboard-in-the-loop @ 60 Hz.
- Debug/calibration always available on leader USB CDC @ 1 Mbaud (PlatformIO is source of truth for baud).
- Replace periodic ESP-NOW “presence spam” with a **central link heartbeat** (any inbound packet resets the timer).
- Remove obsolete / bench-only paths that fight production modes.

**Reference diagrams**

- [architecture/communication_links.md](architecture/communication_links.md)
- [architecture/communication_links.svg](architecture/communication_links.svg)

---

## Phase 0 — Hygiene (before feature work)

- [x] Fix dashboard `telemetry_com_mirror.py` syntax (paren)
- [x] Factorize `drainLatestBatch` (shared teleop ingest helper)
- [x] USB CDC baud from `platformio.ini` (`USB_CDC_BAUD`), remove `kPassthroughUsbBaud` duplicate
- [ ] Audit profile/transport matrix (table below) on hardware; note what to delete
- [ ] Add CI/dashboard `py_compile` for telemetry tools

### Profile / transport audit (current firmware)

| Profile | ID | Transport | Wi-Fi needed? | Verdict |
|---------|----|-----------|---------------|---------|
| CalibrationLeader | 0 | ESP-NOW commands + USB debug | No (cal via USB + ESP-NOW) | **Keep** — USB protocol must stay |
| CalibrationFollower | 1 | ESP-NOW commands | No | **Keep** |
| TeleopEspNow | 2 | ESP-NOW batches | No (OTA optional) | **Keep** — disable Wi-Fi STA during teleop |
| TeleopWifi | 3 | UDP via router | Yes (today) | **Replace** with AP/STA direct |
| Passthrough | 4 | USB ↔ servo bus | No | **Keep** (debug/calib tool) |
| TeleopPcSerial | 5 | Wi-Fi snapshot → PC → USB follower | Yes + PC hop | **Remove** after AP path works |

| Artifact | Verdict |
|----------|---------|
| `telemetry_com_mirror.py` / dashboard COM mirror UI | **Remove** with profile 5 |
| `LeaderTeleopPcSerialBridge` / `FollowerTeleopPcSerialBridge` | **Remove** after profile 5 gone |
| Wi-Fi telemetry stream @ 10–250 ms during teleop | **Debug only** — not in hot path |
| `serial_teleop_bridge.py` raw passthrough | **Delete** if still present |

---

## Phase 1 — Link heartbeat manager (replace presence cadence)

**Problem:** Periodic full presence frames while boards already exchange teleop/commands wastes airtime and complicates “linked” state.

**Design**

- [ ] Add `common/link/link_heartbeat_manager.h` — single API for leader + follower
  - `notifyInboundActivity()` on **any** valid ESP-NOW (or TCP) frame from peer
  - `tick(nowMs)` — if idle longer than `kLinkHeartbeatIntervalMs`, send **small** heartbeat (not full presence)
  - `isPeerAlive(nowMs)` — used instead of “recent presence frame”
- [ ] Shrink presence packet: pairing + IP hint + ACK fields only when needed; no redundant telemetry in hot path
- [ ] Leader: stop using `isFollowerLinked()` = “got presence recently” only; include teleop/command RX as liveness
- [ ] Follower: remove `kPresenceTxPeriodTeleopMs` / `kPresenceTxPeriodWifiTeleopMs` special cases; heartbeat manager decides TX
- [ ] Pairing: keep MAC lock + `PairRequest` / accept; drop “presence watchdog” hacks (`refreshFollowerLinkGrace` layering)
- [ ] Update [teleop_performance.md](teleop_performance.md) ESP-NOW section
- [ ] Update [architecture/README.md](architecture/README.md) message flow

**Exit criteria:** ESP-NOW teleop @ 60 Hz with presence TX ≤ 1 Hz when batches flow; link stays up during cal center.

---

## Phase 2 — ESP-NOW teleop: Wi-Fi off (except OTA)

- [ ] `TeleopEspNow` profile: `WiFi.mode(WIFI_OFF)` or STA disconnect after boot handshake; keep OTA entry (button / timed window / `WiFi.begin` on demand)
- [ ] Leader mirror task: only ESP-NOW `sendBatch` (already true); verify no mDNS/UDP side effects
- [ ] Follower: no Wi-Fi UDP listener active in this profile
- [ ] Dashboard: leader connected via **USB serial** debug channel (not `:9090` for teleop)
- [ ] Document OTA procedure when Wi-Fi was off
- [ ] Bench: measure latency/jitter vs current

---

## Phase 3 — USB debug channel (calibration + dashboard)

**Problem:** PC serial mirror duplicated leader positions over Wi-Fi → PC → USB (choppy, unfair test).

- [ ] Define `debug_protocol` on USB CDC @ `USB_CDC_BAUD` (1M): commands, snapshots, pairing status (binary, not 96-char text fields)
- [ ] Calibration leader/follower: all capture/center/scan over USB debug + ESP-NOW to follower (unchanged semantics, simpler frames)
- [ ] Dashboard connects to leader COM only; no Wi-Fi requirement for cal/debug
- [ ] Throttle or disable `:9090` stream unless “Wi-Fi debug” explicitly enabled
- [ ] Update dashboard `telemetry_client.py` for new debug transport (or dual stack during migration)

---

## Phase 4 — Wi-Fi direct teleop (AP/STA)

- [ ] ESP-NOW side channel: negotiate WPA2 PSK + role (leader AP / follower STA) + TCP/UDP port
- [ ] Leader `TeleopWifi` profile: bring up soft-AP; follower joins; **no home router** in mirror path
- [ ] Teleop batches over UDP or TCP (reuse `teleop_wifi::BatchPacket`, latest-only, fire-and-forget)
- [ ] Heartbeat over same link (or ESP-NOW keepalive when Wi-Fi teleop idle)
- [ ] Radio budget: no concurrent router STA + AP unless OTA; document channel selection
- [ ] Remove router-based follower IP discovery from hot path (`soarm-follower.local` fallback = debug only)
- [ ] Update [communication_links.md](architecture/communication_links.md) mode 4

---

## Phase 5 — Remove obsolete modes & code

- [ ] Remove `ControllerOperationProfile::TeleopPcSerial` and transport `PcSerialBridge`
- [ ] Remove `telemetry_com_mirror.py`, COM mirror API routes, UI in `mode_view.js`
- [ ] Remove leader/follower PC serial bridge classes
- [ ] Remove dashboard Wi-Fi→COM mirror from `start_dashboard.ps1` flags
- [ ] Shrink `controller_operation_profile` count; migrate Xbox cycle order
- [ ] Grep cleanup: `PcSerial`, `com-mirror`, `kTelemetrySnapshotPcSerialMirrorPeriodMs`
- [ ] Native tests / structural limits green

---

## Phase 6 — Documentation & validation

- [ ] [architecture/README.md](architecture/README.md) — single story per mode
- [ ] [CODE_CONTEXT_INDEX.md](CODE_CONTEXT_INDEX.md) — new files, deleted files
- [ ] [teleop_performance.md](teleop_performance.md) — bench results table (ESP-NOW vs Wi-Fi direct vs USB debug)
- [ ] Salon checklist: cal both arms USB, teleop ESP-NOW Wi-Fi off, teleop Wi-Fi direct, OTA
- [ ] README leader/follower flash instructions

---

## Why PC serial mirror failed (keep for posterity)

1. **Triple hop:** leader bus → Wi-Fi snapshot → Python → USB → follower (jitter at each step).
2. **Telemetry still active:** even throttled to 250 ms, dashboard + binary snapshot competes with COM writes on the PC.
3. **Wrong baud on follower monitor:** PlatformIO `monitor_speed` for follower env is 115200 while teleop batches expect USB CDC rate; host must match `USB_CDC_BAUD`.
4. **Not comparable to LeRobot:** LeRobot owns both USB ports synchronously; we async’d through a web stack.

Do not invest further in profile 5; delete in Phase 5.

---

## Suggested execution order

```
Phase 0 → Phase 1 (heartbeat) → Phase 2 (ESP-NOW Wi-Fi off)
        → Phase 3 (USB debug) → Phase 4 (Wi-Fi AP/STA) → Phase 5 (delete dead code) → Phase 6 (docs)
```

Parallel safe: Phase 3 (USB) can start while Phase 1 lands; Phase 4 depends on Phase 1 heartbeat for link state during AP bring-up.

---

## Notes

- **Servo speed profiles:** keep for fast STS; default teleop speed % for current hardware stays moderate.
- **Passthrough:** remains profile 4; baud = `USB_CDC_BAUD` from PlatformIO (same as monitor for leader).
- **Commits:** user commits baseline before refactor branches; tick checkboxes in PR descriptions.

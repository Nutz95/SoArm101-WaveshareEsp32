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
- [x] Add dashboard `py_compile` helper (`tools/check_dashboard_syntax.py`)

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

## Phase 1 — Link heartbeat manager (replace presence cadence) ✅

**Problem:** Periodic full presence frames while boards already exchange teleop/commands wastes airtime and complicates “linked” state.

**Design**

- [x] Add `common/link/link_heartbeat_manager.h` — `notifyPeerActivity`, `shouldSendHeartbeat`, `isPeerAlive`
- [x] Add compact `LinkHeartbeatPacket` + `PresenceMessageType::LinkHeartbeat` (no telemetry strings on hot path)
- [x] Leader: `isFollowerLinked()` via heartbeat manager; inbound ESP-NOW + `notifyPeerLinkActivity()` on command TX
- [x] Follower: removed `kPresenceTxPeriodTeleopMs` / `kPresenceTxPeriodWifiTeleopMs`; TX gated by teleop traffic + heartbeat intervals
- [x] `refreshFollowerLinkGrace()` → `notifyPeerLinkActivity()` (same call sites)
- [x] Update [teleop_performance.md](teleop_performance.md) ESP-NOW section
- [x] Update [architecture/README.md](architecture/README.md) message flow

**Exit criteria:** ESP-NOW teleop @ 60 Hz with outbound presence ≤ 1 Hz when batches flow; link stays up during cal center (bench after flash).

---

## Phase 2 — ESP-NOW teleop: Wi-Fi STA off (except OTA)

- [x] `WifiOtaService::setStaConnectDesired` — **no** `WiFi.disconnect()` (panics with ESP-NOW + NimBLE); STA may stay up
- [x] `TeleopEspNow` profile (leader): STA off + telemetry stream `:9090` listener paused
- [x] Follower: STA off when ESP-NOW teleop active and no recent Wi-Fi teleop
- [x] OTA start forces `WiFi.begin` again
- [x] Xbox profile **OtaReady** (cycle manette) forces STA on for `pio run -e *-ota`
- [ ] Dashboard: leader debug via **USB serial** when `:9090` paused (TeleopEspNow) — blocked on Phase 3 `debug_protocol`
- [x] Document OTA when STA disconnected (see [communication_links.md](architecture/communication_links.md))
- [ ] Bench: measure latency/jitter vs current

---

## Phase 3 — USB debug channel (calibration + dashboard)

**Problem:** PC serial mirror duplicated leader positions over Wi-Fi → PC → USB (choppy, unfair test).

- [x] USB CDC reuses dashboard command + snapshot binary (`LeaderUsbDebugService`, same magic as `:9090`)
- [x] Calibration leader/follower over USB debug + ESP-NOW to follower (dashboard `teleop_calibration_capture` / transport set)
- [x] Dashboard `--leader-serial COMx` (`telemetry_serial_client.py`) — no Wi-Fi for cal/debug when TeleopEspNow pauses TCP
- [x] `start_dashboard.ps1` reads `monitor_port` / `USB_CDC_BAUD` from `platformio.ini` by default
- [x] Leader text logs suppressed on USB while dashboard stream is active (`usb_debug_log_gate`)
- [x] `:9090` still paused in TeleopEspNow; USB path always active in `LeaderApp::tick`
- [x] Document Xbox BLE + USB debug in [architecture/xbox_ble_controls.md](architecture/xbox_ble_controls.md)

---

## Phase 4 — Wi-Fi direct teleop (AP/STA)

- [x] ESP-NOW binary offer/ack (`WifiDirectOfferPacket` / `WifiDirectAckPacket`) with random per-session PSK
- [x] Leader `TeleopWifi` profile: soft-AP + offer over ESP-NOW (`LeaderWifiDirectSession`)
- [x] Follower STA join + binary ack with assigned IP (`FollowerWifiDirectLink`)
- [x] Teleop batches stay binary UDP (`teleop_wifi::BatchPacket`) on negotiated link
- [x] OTA / other profiles tear down direct session and restore home STA when leaving `TeleopWifi`
- [x] Native unit tests: `test/test_wifi_direct_session`
- [ ] Bench: latency/jitter vs ESP-NOW on hardware
- [ ] Remove router-based follower IP discovery from hot path (`soarm-follower.local` fallback = debug only)
- [ ] Update [communication_links.md](architecture/communication_links.md) mode 4 (bench results)

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

- **Leader BLE (2026-05):** `env:leader` / `leader-ota` / `build_upload_leader.ps1` enable Xbox BLE by default; use `leader-no-ble` or `-NoBle` to opt out.
- **Servo speed profiles:** keep for fast STS; default teleop speed % for current hardware stays moderate.
- **Passthrough:** remains profile 4; baud = `USB_CDC_BAUD` from PlatformIO (same as monitor for leader).
- **Commits:** user commits baseline before refactor branches; tick checkboxes in PR descriptions.

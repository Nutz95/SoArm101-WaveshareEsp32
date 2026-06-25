# ESP-NOW pairing (leader ↔ follower)

Pairing binds one **follower MAC** to the **leader** so teleop batches and commands are accepted. It uses the same ESP-NOW presence link as telemetry — **no router required**.

## First-time pairing (automatic, no dashboard)

1. Flash **leader** and **follower** (USB). Pairing NVS is empty on first boot.
2. Power **both** boards within ESP-NOW range (same room, antennas unobstructed).
3. **Follower** broadcasts `PairRequest` every few seconds while unpaired.
4. **Leader** (unpaired) accepts the first valid request, sends `PairAck`, saves follower MAC in NVS.
5. **Follower** receives `PairAck`, saves leader MAC in NVS.
6. Leader OLED / status: linked when presence resumes. Teleop (**A**) works once Xbox is connected.

Typical time: **a few seconds** after both boards boot. No button press required.

```text
Follower                         Leader
   |  PairRequest (broadcast)  -->  |  accept (not paired yet)
   |  <--  PairAck                |
   |  save leader MAC             |  save follower MAC
   |  linked                      |  linked
```

## Without the HTML dashboard

| Task | Dashboard? | How |
|------|------------|-----|
| **First pair** | No | Automatic (above) |
| **Teleop** | No | Xbox **A** / **B** on leader |
| **Reset pairing** | Optional | See below |
| **Force follower unpair** | No | USB factory reset flash |

### Reset pairing without dashboard

**Option A — Leader USB serial** (same binary protocol as Wi-Fi `:9090`):

1. Connect leader USB, start dashboard in serial mode:

```powershell
cd ESP32/tools/telemetry_dashboard
python telemetry_dashboard.py --leader-serial COM7 --leader-serial-baud 115200
```

2. Open http://127.0.0.1:8080 → **Pairing** → **Reset pairing**.

Leader clears NVS, sends `PairReset` to the old follower; follower clears its NVS and sends `PairRequest` again.

**Option B — Wi-Fi dashboard** (needs router + SSID in firmware):

`.\start_dashboard.ps1` → Pairing → Reset pairing.

**Option C — Follower flash erase** (stuck / wrong leader):

```powershell
.\build_upload_follower.ps1 -FactoryResetPairing -UploadPort COM8
```

Erases follower flash including pairing. Leader may still remember the old follower MAC — use **Option A or B** on the leader, or erase/reflash leader NVS if needed.

**Option D — Pairing timeout**

If the leader loses contact with the paired follower for **45 s** (`kPairingTimeoutMs`), it **expires** pairing automatically and accepts a new `PairRequest`.

## Pairing a different follower

1. Reset pairing on the **leader** (Option A or B).
2. Power the new follower (or factory-reset the old one).
3. Wait for automatic re-pair.

Only **one** follower MAC is stored on the leader at a time.

## Xbox controller (separate from ESP-NOW)

ESP-NOW pairing is **between the two ESP32 boards**. The **Xbox** pairs to the **leader only** over BLE on each leader boot (full scan ~4 s). See [architecture/xbox_ble_controls.md](architecture/xbox_ble_controls.md).

## LED / OLED hints

| Signal | Meaning |
|--------|---------|
| Follower LED / serial `[FOLLOWER] Paired with leader` | ESP-NOW paired |
| Leader `connecting` / follower IP on OLED | Router STA (optional), not pairing state |
| Teleop **A** ignored, no mirror | Check ESP-NOW link + Xbox BLE connected |

## Code references

| Piece | Path |
|-------|------|
| Pair policy | `src/common/pairing/pairing_policy.cpp` |
| Leader handlers | `src/leader/leader_presence_service_handlers.cpp` |
| Follower RX | `src/follower/follower_presence_inbound.cpp` |
| Reset command | `src/leader/leader_app_commands.cpp` (`resetPairing`) |
| NVS MAC store | `src/common/peer_pairing_store.h` |

# Salon flash cheat sheet

PlatformIO remembers the **last environment** and sometimes the **last upload port**. After flashing the follower on COM8, a leader USB upload can wrongly target COM8 until you restart the IDE.

## Reliable USB flash (recommended)

From repo root:

```powershell
.\ESP32\build_upload_leader.ps1 -Port COM7
.\ESP32\build_upload_follower.ps1 -Port COM8
```

Scripts pass `--upload-port` explicitly so the port does not depend on IDE cache.

Manual equivalent:

```powershell
cd ESP32
pio run -e leader -t upload --upload-port COM7
pio run -e follower -t upload --upload-port COM8
```

## OTA flash

1. Power boards; PC on the same LAN as the salon router.
2. Leader: cycle Xbox profile to **OTA**, press **A** (OLED: `OTA ACTIVE`, router IP).
3. Upload:

```powershell
.\ESP32\build_upload_leader.ps1 -Ota
# or with fixed IP:
.\ESP32\build_upload_leader.ps1 -Ota -OtaIp 192.168.x.y
```

Follower has no Xbox menu — leave it powered on home Wi‑Fi (or flash follower USB once).

```powershell
pio run -e follower-ota -t upload --upload-port soarm-follower.local
```

If OTA fails after Wi‑Fi direct teleop, cycle to **OTA** and press **A** once (forces clean home STA + `ArduinoOTA`).

## Wi‑Fi direct without router

Salon Wi‑Fi down is OK if leader and follower are **ESP‑NOW paired** (NVS):

1. Boot both arms (no router IP is fine).
2. Leader: **Teleop Wi‑Fi** → **A** (AP `192.168.4.1`).
3. Wait for follower join (`waiting follower` → `Start? (A)`).
4. **A** again to start UDP teleop.

Pairing uses ESP‑NOW only; router is not required for the direct link.

## Monitor

```powershell
cd ESP32
pio device monitor -e leader -b 115200
pio device monitor -e follower -b 1000000
```

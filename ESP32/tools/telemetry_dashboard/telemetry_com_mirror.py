"""Mirror leader arm positions to the follower over USB COM (WiFi telemetry -> follower serial)."""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover
    serial = None  # type: ignore
    list_ports = None  # type: ignore

from dashboard_state import DashboardState
from teleop_batch_codec import pack_teleop_batch
from teleop_runtime import parse_servo_ids, parse_servo_telemetry


def list_serial_ports() -> List[Dict[str, str]]:
    if list_ports is None:
        return []
    ports: List[Dict[str, str]] = []
    for info in list_ports.comports():
        ports.append(
            {
                "device": info.device,
                "description": info.description or "",
                "hwid": info.hwid or "",
            }
        )
    return ports


def _remap_position(
    servo_id: int,
    source_position: int,
    source_min: int,
    source_max: int,
    target_min: int,
    target_max: int,
) -> int:
    if source_max <= source_min or target_max <= target_min:
        return source_position
    normalized = (float(source_position) - float(source_min)) / float(source_max - source_min)
    normalized = max(0.0, min(1.0, normalized))
    mapped = float(target_min) + normalized * float(target_max - target_min)
    return int(mapped + 0.5)


def build_mirror_batch_entries(snapshot: Dict[str, Any], speed_pct: int) -> List[Tuple[int, int]]:
    del speed_pct
    leader_ids = parse_servo_ids(str(snapshot.get("leader_servo_ids", "")))
    follower_ids = set(parse_servo_ids(str(snapshot.get("follower_servo_ids", ""))))

    leader_min = snapshot.get("leader_calibration_min") or [0] * 6
    leader_max = snapshot.get("leader_calibration_max") or [4095] * 6
    follower_min = snapshot.get("follower_calibration_min") or [0] * 6
    follower_max = snapshot.get("follower_calibration_max") or [4095] * 6

    mirror_positions = snapshot.get("leader_mirror_positions")
    mirror_count = int(snapshot.get("leader_mirror_position_count", 0))
    use_binary_mirror = (
        isinstance(mirror_positions, list)
        and len(mirror_positions) >= 6
        and mirror_count > 0
    )

    leader_telemetry = None
    if not use_binary_mirror:
        leader_telemetry = parse_servo_telemetry(str(snapshot.get("leader_servo_telemetry", "")))

    entries: List[Tuple[int, int]] = []
    for servo_id in leader_ids:
        if servo_id not in follower_ids:
            continue
        index = servo_id - 1
        if index < 0 or index >= 6:
            continue

        if use_binary_mirror:
            raw_position = int(mirror_positions[index])
            if raw_position <= -32768:
                continue
        else:
            telemetry = leader_telemetry.get(servo_id) if leader_telemetry is not None else None
            if telemetry is None:
                continue
            raw_position = int(telemetry["position"])

        position = _remap_position(
            servo_id,
            raw_position,
            int(leader_min[index]),
            int(leader_max[index]),
            int(follower_min[index]),
            int(follower_max[index]),
        )
        entries.append((servo_id, position))
    return entries


@dataclass
class ComMirrorStats:
    packets_sent: int = 0
    idle_reason: str = "stopped"
    last_error: str = ""
    follower_port_open: bool = False
    last_send_monotonic: float = 0.0


@dataclass
class ComMirrorConfig:
    follower_port: str = "COM8"
    follower_baud: int = 115200
    speed_pct: int = 100
    target_hz: float = 60.0


class TelemetryComMirror:
    """Send teleop batch frames to the follower COM port from dashboard WiFi telemetry."""

    def __init__(self, state: DashboardState, config: ComMirrorConfig) -> None:
        self._state = state
        self._config = config
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._stats = ComMirrorStats()
        self._running = False
        self._serial = None
        self._request_id = 0

    @property
    def running(self) -> bool:
        return self._running

    def configure(self, config: ComMirrorConfig) -> None:
        if self._running:
            raise RuntimeError("stop the COM mirror before changing ports")
        self._config = config

    def snapshot(self) -> Dict[str, Any]:
        return {
            "mode": "telemetry_com_mirror",
            "running": self._running,
            "follower_port": self._config.follower_port,
            "follower_baud": self._config.follower_baud,
            "speed_pct": self._config.speed_pct,
            "packets_sent": self._stats.packets_sent,
            "packets_forwarded": self._stats.packets_sent,
            "bytes_read": 0,
            "idle_reason": self._stats.idle_reason,
            "last_error": self._stats.last_error,
            "follower_port_open": self._stats.follower_port_open,
            "available_ports": list_serial_ports(),
        }

    def start(self) -> None:
        if serial is None:
            raise RuntimeError("pyserial is required: pip install pyserial")
        if self._running:
            return

        self._stop.clear()
        self._stats = ComMirrorStats()
        try:
            self._serial = serial.Serial(
                self._config.follower_port,
                self._config.follower_baud,
                timeout=0.05,
                write_timeout=1.0,
            )
            self._stats.follower_port_open = True
            print(
                f"[com-mirror] OPEN OK follower={self._config.follower_port} "
                f"@{self._config.follower_baud} (WiFi telemetry -> follower USB)"
            )
        except Exception as exc:
            self._stats.last_error = str(exc)
            self._stats.idle_reason = "port_open_failed"
            print(f"[com-mirror] OPEN FAIL follower={self._config.follower_port}: {exc}")
            raise RuntimeError(str(exc)) from exc

        self._thread = threading.Thread(target=self._run_loop, name="telemetry-com-mirror", daemon=True)
        self._thread.start()
        self._running = True
        self._stats.idle_reason = "waiting_teleop_continuous"

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None
        if self._serial is not None and self._serial.is_open:
            self._serial.close()
        self._serial = None
        self._running = False
        self._stats.follower_port_open = False
        self._stats.idle_reason = "stopped"
        print("[com-mirror] stopped")

    def _run_loop(self) -> None:
        period_s = 1.0 / max(1.0, self._config.target_hz)
        try:
            while not self._stop.is_set():
                snapshot = self._state.snapshot()
                if not snapshot.get("connected"):
                    self._stats.idle_reason = "leader_wifi_down"
                    time.sleep(0.1)
                    continue

                if int(snapshot.get("controller_operation_profile", 2)) != 5:
                    self._stats.idle_reason = "profile_not_pc_serial"
                    time.sleep(0.05)
                    continue

                if not snapshot.get("teleop_continuous_enabled"):
                    self._stats.idle_reason = "teleop_continuous_off"
                    time.sleep(0.05)
                    continue

                entries = build_mirror_batch_entries(snapshot, self._config.speed_pct)
                if not entries:
                    self._stats.idle_reason = "no_mirrorable_servos"
                    time.sleep(0.05)
                    continue

                self._request_id = (self._request_id + 1) & 0xFFFF
                if self._request_id == 0:
                    self._request_id = 1
                packet = pack_teleop_batch(entries, self._config.speed_pct, self._request_id)
                if self._serial is None or not self._serial.is_open:
                    self._stats.idle_reason = "follower_port_closed"
                    break

                try:
                    self._serial.write(packet)
                    self._serial.flush()
                except Exception as exc:
                    self._stats.last_error = str(exc)
                    self._stats.idle_reason = "write_failed"
                    print(f"[com-mirror] WRITE FAIL: {exc}")
                    break

                self._stats.packets_sent += 1
                self._stats.last_send_monotonic = time.monotonic()
                self._stats.idle_reason = "sending"

                if self._stats.packets_sent == 1:
                    print(f"[com-mirror] first batch sent ({len(entries)} servos)")
                elif self._stats.packets_sent % 120 == 0:
                    print(f"[com-mirror] batches sent={self._stats.packets_sent}")

                time.sleep(period_s)
        finally:
            self._running = False
            if self._serial is not None and self._serial.is_open:
                self._serial.close()
            self._serial = None
            self._stats.follower_port_open = False

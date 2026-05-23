import threading
import time
from typing import Dict, Any


class DashboardState:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._latest: Dict[str, Any] = {
            "uptime_ms": 0,
            "cpu0_load_pct": 0,
            "cpu1_load_pct": 0,
            "leader_state": 0,
            "follower_state": 0,
            "mode": 0,
            "joystick_paired": False,
            "calibration_done": False,
            "espnow_linked": False,
            "pairing_locked": False,
            "leader_servo_debug_manual": False,
            "follower_servo_debug_manual": False,
            "leader_servo_count": 0,
            "follower_servo_count": 0,
            "leader_ip": "",
            "follower_ip": "",
            "leader_mac": "",
            "follower_mac": "",
            "leader_servo_ids": "",
            "follower_servo_ids": "",
            "leader_servo_telemetry": "",
            "follower_servo_telemetry": "",
            "command_request_id": 0,
            "command_code": 0,
            "leader_command_status": 0,
            "follower_command_status": 0,
            "status": "",
            "connected": False,
            "last_frame_ts": 0.0,
        }

    def snapshot(self) -> Dict[str, Any]:
        with self._lock:
            return dict(self._latest)

    def set_connected(self, value: bool) -> None:
        with self._lock:
            self._latest["connected"] = value

    def update(self, updates: Dict[str, Any]) -> None:
        with self._lock:
            self._latest.update(updates)
            self._latest["last_frame_ts"] = time.time()

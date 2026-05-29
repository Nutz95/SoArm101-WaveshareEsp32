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
            "follower_ack_retries_used": 0,
            "follower_ack_rtt_ms": 0,
            "follower_ack_timeout_count": 0,
            "follower_ack_pending": False,
            "teleop_mirror_latency_last_ms": 0,
            "teleop_mirror_latency_ewma_ms": 0,
            "teleop_mirror_latency_p95_ms": 0,
            "teleop_mirror_pending_count": 0,
            "teleop_mirror_timeout_count": 0,
            "teleop_continuous_enabled": False,
            "teleop_continuous_servo_id": 0,
            "teleop_transport_mode": 0,
            "xbox_runtime_state": 0,
            "xbox_last_report_age_ms": 0,
            "xbox_report_count": 0,
            "xbox_buttons_mask": 0,
            "xbox_axis_left_x": 0,
            "xbox_axis_left_y": 0,
            "xbox_axis_right_x": 0,
            "xbox_axis_right_y": 0,
            "xbox_dpad_x": 0,
            "xbox_dpad_y": 0,
            "xbox_trigger_left": 0,
            "xbox_trigger_right": 0,
            "xbox_link_encrypted": False,
            "xbox_input_subscribed": False,
            "xbox_controller_paired": False,
            "xbox_controller_name": "",
            "leader_state": 0,
            "follower_state": 0,
            "mode": 0,
            "joystick_paired": False,
            "calibration_done": False,
            "espnow_linked": False,
            "pairing_locked": False,
            "leader_servo_debug_manual": False,
            "follower_servo_debug_manual": False,
            "leader_servo_temperature_alarm": False,
            "follower_servo_temperature_alarm": False,
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
            "controller_operation_profile": 2,
            "calibration_phase": 0,
            "leader_calibration_min": [0] * 6,
            "leader_calibration_max": [4095] * 6,
            "follower_calibration_min": [0] * 6,
            "follower_calibration_max": [4095] * 6,
            "leader_working_calibration_min": [4095] * 6,
            "leader_working_calibration_max": [0] * 6,
            "follower_working_calibration_min": [4095] * 6,
            "follower_working_calibration_max": [0] * 6,
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

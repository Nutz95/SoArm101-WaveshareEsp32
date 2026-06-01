"""Leader telemetry over USB serial (Phase 3 — same protocol as TCP :9090)."""

import threading
import time
from typing import Optional

import serial

from dashboard_protocol import (
    CMD_STRUCT,
    ESP_CMD_MAGIC,
    ESP_CMD_START_STREAM,
    ESP_CMD_STOP_STREAM,
    ESP_CMD_VERSION,
    ESP_TLM_MAGIC,
    ESP_TLM_TYPE,
    ESP_TLM_VERSION,
    SNAPSHOT_STRUCT,
    TLM_HEADER_STRUCT,
    decode_cstr,
)
from dashboard_state import DashboardState


class TelemetrySerialClient(threading.Thread):
    STALE_STREAM_TIMEOUT_S = 10.0

    def __init__(self, port: str, baud: int, state: DashboardState):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.state = state
        self.stop_event = threading.Event()
        self._serial_lock = threading.Lock()
        self._serial: Optional[serial.Serial] = None
        self._last_frame_monotonic = 0.0

    def send_command(self, command: int, value: int = 0, request_id: int = 0) -> bool:
        frame = CMD_STRUCT.pack(ESP_CMD_MAGIC, ESP_CMD_VERSION, command, request_id & 0xFFFF, value)
        with self._serial_lock:
            if self._serial is None or not self._serial.is_open:
                return False
            try:
                self._serial.write(frame)
                self._serial.flush()
                return True
            except Exception:
                return False

    def run(self) -> None:
        while not self.stop_event.is_set():
            try:
                self._run_session()
            except Exception:
                self.state.set_connected(False)
                self._close_serial()
                time.sleep(1.0)

    def _run_session(self) -> None:
        with serial.Serial(self.port, self.baud, timeout=0.05) as ser:
            self._set_serial(ser)
            self.state.set_connected(True)
            self._last_frame_monotonic = time.monotonic()
            self.send_command(ESP_CMD_START_STREAM)

            rx = bytearray()
            while not self.stop_event.is_set():
                if (time.monotonic() - self._last_frame_monotonic) > self.STALE_STREAM_TIMEOUT_S:
                    break

                chunk = ser.read(256)
                if chunk:
                    rx.extend(chunk)
                else:
                    time.sleep(0.01)

                while len(rx) >= TLM_HEADER_STRUCT.size:
                    if not self._try_consume_frame(rx):
                        break

            try:
                self.send_command(ESP_CMD_STOP_STREAM)
            except Exception:
                pass

        self._close_serial()
        self.state.set_connected(False)

    def _try_consume_frame(self, rx: bytearray) -> bool:
        if len(rx) < TLM_HEADER_STRUCT.size:
            return False

        magic, version, packet_type = TLM_HEADER_STRUCT.unpack_from(rx, 0)
        if magic != ESP_TLM_MAGIC or version != ESP_TLM_VERSION or packet_type != ESP_TLM_TYPE:
            del rx[0]
            return True

        total = TLM_HEADER_STRUCT.size + SNAPSHOT_STRUCT.size
        if len(rx) < total:
            return False

        payload = bytes(rx[TLM_HEADER_STRUCT.size : total])
        del rx[:total]
        self._update_snapshot(payload)
        self._last_frame_monotonic = time.monotonic()
        return True

    def _set_serial(self, ser: serial.Serial) -> None:
        with self._serial_lock:
            self._serial = ser

    def _close_serial(self) -> None:
        with self._serial_lock:
            if self._serial is not None and self._serial.is_open:
                self._serial.close()
            self._serial = None

    def _update_snapshot(self, payload: bytes) -> None:
        fields = SNAPSHOT_STRUCT.unpack(payload)
        self.state.update(
            {
                "uptime_ms": int(fields[0]),
                "cpu0_load_pct": int(fields[1]),
                "cpu1_load_pct": int(fields[2]),
                "follower_ack_retries_used": int(fields[3]),
                "follower_ack_rtt_ms": int(fields[4]),
                "follower_ack_timeout_count": int(fields[5]),
                "follower_ack_pending": bool(fields[6]),
                "teleop_mirror_latency_last_ms": int(fields[7]),
                "teleop_mirror_latency_ewma_ms": int(fields[8]),
                "teleop_mirror_latency_p95_ms": int(fields[9]),
                "teleop_mirror_pending_count": int(fields[10]),
                "teleop_mirror_timeout_count": int(fields[11]),
                "teleop_continuous_enabled": bool(fields[12]),
                "teleop_continuous_servo_id": int(fields[13]),
                "teleop_transport_mode": int(fields[14]),
                "xbox_runtime_state": int(fields[15]),
                "xbox_last_report_age_ms": int(fields[16]),
                "xbox_report_count": int(fields[17]),
                "xbox_buttons_mask": int(fields[18]),
                "xbox_axis_left_x": int(fields[19]),
                "xbox_axis_left_y": int(fields[20]),
                "xbox_axis_right_x": int(fields[21]),
                "xbox_axis_right_y": int(fields[22]),
                "xbox_dpad_x": int(fields[23]),
                "xbox_dpad_y": int(fields[24]),
                "xbox_trigger_left": int(fields[25]),
                "xbox_trigger_right": int(fields[26]),
                "leader_state": int(fields[27]),
                "follower_state": int(fields[28]),
                "mode": int(fields[29]),
                "xbox_link_encrypted": bool(fields[30]),
                "xbox_input_subscribed": bool(fields[31]),
                "joystick_paired": bool(fields[32]),
                "xbox_controller_paired": bool(fields[33]),
                "calibration_done": bool(fields[34]),
                "espnow_linked": bool(fields[35]),
                "pairing_locked": bool(fields[36]),
                "leader_servo_debug_manual": bool(fields[37]),
                "follower_servo_debug_manual": bool(fields[38]),
                "leader_servo_temperature_alarm": bool(fields[39]),
                "follower_servo_temperature_alarm": bool(fields[40]),
                "leader_servo_count": int(fields[41]),
                "follower_servo_count": int(fields[42]),
                "leader_ip": decode_cstr(fields[43]),
                "follower_ip": decode_cstr(fields[44]),
                "leader_mac": decode_cstr(fields[45]),
                "follower_mac": decode_cstr(fields[46]),
                "leader_servo_ids": decode_cstr(fields[47]),
                "follower_servo_ids": decode_cstr(fields[48]),
                "leader_servo_telemetry": decode_cstr(fields[49]),
                "follower_servo_telemetry": decode_cstr(fields[50]),
                "xbox_controller_name": decode_cstr(fields[51]),
                "command_request_id": int(fields[52]),
                "command_code": int(fields[53]),
                "leader_command_status": int(fields[54]),
                "follower_command_status": int(fields[55]),
                "status": decode_cstr(fields[56]),
                "controller_operation_profile": int(fields[57]),
                "calibration_phase": int(fields[58]),
                "leader_calibration_min": [int(fields[59 + i]) for i in range(6)],
                "leader_calibration_max": [int(fields[65 + i]) for i in range(6)],
                "follower_calibration_min": [int(fields[71 + i]) for i in range(6)],
                "follower_calibration_max": [int(fields[77 + i]) for i in range(6)],
                "leader_working_calibration_min": [int(fields[83 + i]) for i in range(6)],
                "leader_working_calibration_max": [int(fields[89 + i]) for i in range(6)],
                "follower_working_calibration_min": [int(fields[95 + i]) for i in range(6)],
                "follower_working_calibration_max": [int(fields[101 + i]) for i in range(6)],
                "leader_mirror_positions": [int(fields[107 + i]) for i in range(6)],
                "leader_mirror_position_count": int(fields[113]),
            }
        )

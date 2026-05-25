import socket
import threading
import time
from typing import Optional

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


class TelemetryClient(threading.Thread):
    STALE_STREAM_TIMEOUT_S = 3.0

    def __init__(self, host: str, port: int, state: DashboardState):
        super().__init__(daemon=True)
        self.host = host
        self.port = port
        self.state = state
        self.stop_event = threading.Event()
        self._sock_lock = threading.Lock()
        self._sock: Optional[socket.socket] = None
        self._last_frame_monotonic = 0.0

    def send_command(self, command: int, value: int = 0, request_id: int = 0) -> bool:
        frame = CMD_STRUCT.pack(ESP_CMD_MAGIC, ESP_CMD_VERSION, command, request_id & 0xFFFF, value)
        with self._sock_lock:
            if self._sock is None:
                return False
            try:
                self._sock.sendall(frame)
                return True
            except Exception:
                return False

    def run(self) -> None:
        while not self.stop_event.is_set():
            try:
                self._run_session()
            except Exception:
                self.state.set_connected(False)
                self._clear_socket()
                time.sleep(1.0)

    def _run_session(self) -> None:
        with socket.create_connection((self.host, self.port), timeout=5) as sock:
            sock.settimeout(2)
            self._set_socket(sock)
            self.state.set_connected(True)
            self._last_frame_monotonic = time.monotonic()
            self.send_command(ESP_CMD_START_STREAM)

            while not self.stop_event.is_set():
                if (time.monotonic() - self._last_frame_monotonic) > self.STALE_STREAM_TIMEOUT_S:
                    break

                header = self._recv_exact(sock, TLM_HEADER_STRUCT.size)
                if header is None:
                    break

                magic, version, packet_type = TLM_HEADER_STRUCT.unpack(header)
                if magic != ESP_TLM_MAGIC or version != ESP_TLM_VERSION or packet_type != ESP_TLM_TYPE:
                    break

                payload = self._recv_exact(sock, SNAPSHOT_STRUCT.size)
                if payload is None:
                    break

                self._update_snapshot(payload)
                self._last_frame_monotonic = time.monotonic()

            try:
                self.send_command(ESP_CMD_STOP_STREAM)
            except Exception:
                pass

        self._clear_socket()
        self.state.set_connected(False)

    def _set_socket(self, sock: socket.socket) -> None:
        with self._sock_lock:
            self._sock = sock

    def _clear_socket(self) -> None:
        with self._sock_lock:
            self._sock = None

    def _recv_exact(self, sock: socket.socket, size: int):
        buffer = bytearray()
        while len(buffer) < size and not self.stop_event.is_set():
            try:
                chunk = sock.recv(size - len(buffer))
            except socket.timeout:
                if (time.monotonic() - self._last_frame_monotonic) > self.STALE_STREAM_TIMEOUT_S:
                    return None
                continue
            if not chunk:
                return None
            buffer.extend(chunk)
        return bytes(buffer)

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
                "teleop_continuous_enabled": bool(fields[7]),
                "teleop_continuous_servo_id": int(fields[8]),
                "leader_state": int(fields[9]),
                "follower_state": int(fields[10]),
                "mode": int(fields[11]),
                "joystick_paired": bool(fields[12]),
                "calibration_done": bool(fields[13]),
                "espnow_linked": bool(fields[14]),
                "pairing_locked": bool(fields[15]),
                "leader_servo_debug_manual": bool(fields[16]),
                "follower_servo_debug_manual": bool(fields[17]),
                "leader_servo_count": int(fields[18]),
                "follower_servo_count": int(fields[19]),
                "leader_ip": decode_cstr(fields[20]),
                "follower_ip": decode_cstr(fields[21]),
                "leader_mac": decode_cstr(fields[22]),
                "follower_mac": decode_cstr(fields[23]),
                "leader_servo_ids": decode_cstr(fields[24]),
                "follower_servo_ids": decode_cstr(fields[25]),
                "leader_servo_telemetry": decode_cstr(fields[26]),
                "follower_servo_telemetry": decode_cstr(fields[27]),
                "command_request_id": int(fields[28]),
                "command_code": int(fields[29]),
                "leader_command_status": int(fields[30]),
                "follower_command_status": int(fields[31]),
                "status": decode_cstr(fields[32]),
            }
        )

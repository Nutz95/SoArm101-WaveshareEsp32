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
    def __init__(self, host: str, port: int, state: DashboardState):
        super().__init__(daemon=True)
        self.host = host
        self.port = port
        self.state = state
        self.stop_event = threading.Event()
        self._sock_lock = threading.Lock()
        self._sock: Optional[socket.socket] = None

    def send_command(self, command: int, value: int = 0) -> bool:
        frame = CMD_STRUCT.pack(ESP_CMD_MAGIC, ESP_CMD_VERSION, command, value)
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
            self.send_command(ESP_CMD_START_STREAM)

            while not self.stop_event.is_set():
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
                "leader_state": int(fields[5]),
                "follower_state": int(fields[6]),
                "mode": int(fields[7]),
                "joystick_paired": bool(fields[8]),
                "calibration_done": bool(fields[9]),
                "espnow_linked": bool(fields[10]),
                "pairing_locked": bool(fields[11]),
                "leader_ip": decode_cstr(fields[12]),
                "follower_ip": decode_cstr(fields[13]),
                "leader_mac": decode_cstr(fields[14]),
                "follower_mac": decode_cstr(fields[15]),
                "status": decode_cstr(fields[16]),
            }
        )

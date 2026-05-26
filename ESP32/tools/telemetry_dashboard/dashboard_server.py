import json
import mimetypes
import os
from pathlib import Path
from urllib.parse import urlparse, unquote
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Callable, Dict

from dashboard_protocol import (
    ESP_CMD_PING,
    ESP_CMD_RESET_PAIRING,
    ESP_CMD_SERVO_DEBUG_DISABLE,
    ESP_CMD_SERVO_DEBUG_DISABLE_FOLLOWER,
    ESP_CMD_SERVO_DEBUG_ENABLE,
    ESP_CMD_SERVO_DEBUG_ENABLE_FOLLOWER,
    ESP_CMD_SERVO_MOVE,
    ESP_CMD_SERVO_SET_ID,
    ESP_CMD_SERVO_SET_MODE,
    ESP_CMD_SERVO_SCAN,
    ESP_CMD_SERVO_SCAN_FOLLOWER,
    ESP_CMD_SERVO_SCAN_LEADER,
    ESP_CMD_START_STREAM,
    ESP_CMD_STOP_STREAM,
    ESP_CMD_TELEOP_CONTINUOUS_SET,
    ESP_CMD_TELEOP_MIRROR,
    ESP_CMD_TELEOP_TRANSPORT_SET,
)
from controller_runtime import ControllerConfigStore
from dashboard_state import DashboardState
from teleop_runtime import TeleopConfigStore, build_mirror_values, build_teleop_state


def build_dashboard_server(
    bind_host: str,
    port: int,
    state: DashboardState,
    command_sender: Callable[[int, int, int], bool],
) -> HTTPServer:
    class DashboardHandler(BaseHTTPRequestHandler):
        static_dir = Path(os.path.join(os.path.dirname(__file__), "static")).resolve()
        teleop_store = TeleopConfigStore(Path(os.path.join(os.path.dirname(__file__), "teleop_config.json")))
        controller_store = ControllerConfigStore(Path(os.path.join(os.path.dirname(__file__), "controller_config.json")))

        command_map: Dict[str, int] = {
            "start_stream": ESP_CMD_START_STREAM,
            "stop_stream": ESP_CMD_STOP_STREAM,
            "ping": ESP_CMD_PING,
            "reset_pairing": ESP_CMD_RESET_PAIRING,
            "servo_scan": ESP_CMD_SERVO_SCAN,
            "servo_scan_leader": ESP_CMD_SERVO_SCAN_LEADER,
            "servo_scan_follower": ESP_CMD_SERVO_SCAN_FOLLOWER,
            "servo_debug_enable": ESP_CMD_SERVO_DEBUG_ENABLE,
            "servo_debug_disable": ESP_CMD_SERVO_DEBUG_DISABLE,
            "servo_debug_enable_follower": ESP_CMD_SERVO_DEBUG_ENABLE_FOLLOWER,
            "servo_debug_disable_follower": ESP_CMD_SERVO_DEBUG_DISABLE_FOLLOWER,
            "servo_move": ESP_CMD_SERVO_MOVE,
            "servo_set_id": ESP_CMD_SERVO_SET_ID,
            "servo_set_mode": ESP_CMD_SERVO_SET_MODE,
            "teleop_mirror": ESP_CMD_TELEOP_MIRROR,
            "teleop_continuous_set": ESP_CMD_TELEOP_CONTINUOUS_SET,
            "teleop_transport_set": ESP_CMD_TELEOP_TRANSPORT_SET,
        }
        next_request_id: int = 1

        @classmethod
        def allocate_request_id(cls) -> int:
            request_id = cls.next_request_id
            cls.next_request_id = (cls.next_request_id + 1) & 0xFFFF
            if cls.next_request_id == 0:
                cls.next_request_id = 1
            return request_id

        def do_GET(self):
            parsed = urlparse(self.path)
            request_path = parsed.path

            if request_path == "/api/latest":
                payload = json.dumps(state.snapshot()).encode("utf-8")
                self._send_bytes(200, "application/json", payload)
                return

            if request_path == "/api/teleop/state":
                payload = json.dumps(self._build_teleop_payload()).encode("utf-8")
                self._send_bytes(200, "application/json", payload)
                return

            if request_path == "/api/controller/config":
                payload = json.dumps(self._build_controller_payload()).encode("utf-8")
                self._send_bytes(200, "application/json", payload)
                return

            if request_path == "/":
                request_path = "/index.html"

            self._send_static(request_path)
            return

        def do_POST(self):
            parsed = urlparse(self.path)
            if parsed.path == "/api/teleop/config":
                self._handle_teleop_config_post()
                return

            if parsed.path == "/api/teleop/mirror":
                self._handle_teleop_mirror_post()
                return

            if parsed.path == "/api/controller/config":
                self._handle_controller_config_post()
                return

            if parsed.path != "/api/command":
                self.send_response(404)
                self.end_headers()
                return

            payload = self._read_json_body()
            if payload is None:
                self._send_json(400, {"ok": False, "error": "invalid_json"})
                return

            command_name = str(payload.get("command", "")).strip().lower()
            value = int(payload.get("value", 0))

            command_id = self.command_map.get(command_name)
            if command_id is None:
                self._send_json(400, {"ok": False, "error": "unknown_command"})
                return

            request_id = DashboardHandler.allocate_request_id()
            sent = command_sender(command_id, value, request_id)
            self._send_json(200, {"ok": sent, "command": command_name, "request_id": request_id})

        def _handle_teleop_config_post(self) -> None:
            payload = self._read_json_body()
            if payload is None:
                self._send_json(400, {"ok": False, "error": "invalid_json"})
                return

            config = self.teleop_store.update(payload)
            self._send_json(200, {"ok": True, "config": config})

        def _handle_teleop_mirror_post(self) -> None:
            payload = self._read_json_body()
            if payload is None:
                payload = {}

            requested_servo_id = payload.get("servo_id")
            servo_id = None
            if requested_servo_id is not None:
                try:
                    servo_id = int(requested_servo_id)
                except (TypeError, ValueError):
                    self._send_json(400, {"ok": False, "error": "invalid_servo_id"})
                    return

            snapshot = state.snapshot()
            config = self.teleop_store.snapshot()
            packed_values = build_mirror_values(snapshot, config, servo_id)
            if not packed_values:
                self._send_json(400, {"ok": False, "error": "no_mirrorable_servo"})
                return

            request_ids = []
            sent_count = 0
            for packed_value in packed_values:
                request_id = DashboardHandler.allocate_request_id()
                request_ids.append(request_id)
                if command_sender(ESP_CMD_TELEOP_MIRROR, packed_value, request_id):
                    sent_count += 1

            self._send_json(
                200,
                {
                    "ok": sent_count == len(packed_values),
                    "sent_count": sent_count,
                    "requested_count": len(packed_values),
                    "request_ids": request_ids,
                },
            )

        def _handle_controller_config_post(self) -> None:
            payload = self._read_json_body()
            if payload is None:
                self._send_json(400, {"ok": False, "error": "invalid_json"})
                return

            config = self.controller_store.update(payload)
            self._send_json(200, {"ok": True, "config": config})

        def _build_teleop_payload(self) -> Dict[str, object]:
            snapshot = state.snapshot()
            config = self.teleop_store.snapshot()
            return build_teleop_state(snapshot, config)

        def _build_controller_payload(self) -> Dict[str, object]:
            return self.controller_store.state()

        def _read_json_body(self):
            content_length = int(self.headers.get("Content-Length", "0"))
            raw_body = self.rfile.read(content_length) if content_length > 0 else b"{}"
            try:
                return json.loads(raw_body.decode("utf-8"))
            except Exception:
                return None

        def log_message(self, format, *args):
            return

        def _send_json(self, status: int, payload: Dict[str, object]) -> None:
            data = json.dumps(payload).encode("utf-8")
            self._send_bytes(status, "application/json", data)

        def _send_bytes(self, status: int, content_type: str, data: bytes) -> None:
            try:
                self.send_response(status)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
            except (BrokenPipeError, ConnectionAbortedError, ConnectionResetError):
                # Browser canceled request while leader/dashboard state changed.
                return

        def _send_static(self, request_path: str) -> None:
            normalized = unquote(request_path.lstrip("/"))
            if normalized == "":
                normalized = "index.html"

            requested = (self.static_dir / normalized).resolve()
            if not str(requested).startswith(str(self.static_dir)):
                self.send_response(403)
                self.end_headers()
                return

            if not requested.is_file():
                self.send_response(404)
                self.end_headers()
                return

            with requested.open("rb") as file_handle:
                data = file_handle.read()

            content_type = mimetypes.guess_type(str(requested))[0] or "application/octet-stream"
            self._send_bytes(200, content_type, data)

    return HTTPServer((bind_host, port), DashboardHandler)

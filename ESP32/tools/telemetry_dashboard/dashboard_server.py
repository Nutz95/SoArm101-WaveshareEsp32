import json
import os
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Callable, Dict

from dashboard_protocol import (
    ESP_CMD_PING,
    ESP_CMD_RESET_PAIRING,
    ESP_CMD_SERVO_DEBUG_DISABLE,
    ESP_CMD_SERVO_DEBUG_ENABLE,
    ESP_CMD_SERVO_MOVE,
    ESP_CMD_SERVO_SCAN,
    ESP_CMD_START_STREAM,
    ESP_CMD_STOP_STREAM,
)
from dashboard_state import DashboardState


def build_dashboard_server(
    bind_host: str,
    port: int,
    state: DashboardState,
    command_sender: Callable[[int, int], bool],
) -> HTTPServer:
    class DashboardHandler(BaseHTTPRequestHandler):
        static_dir = os.path.join(os.path.dirname(__file__), "static")

        command_map: Dict[str, int] = {
            "start_stream": ESP_CMD_START_STREAM,
            "stop_stream": ESP_CMD_STOP_STREAM,
            "ping": ESP_CMD_PING,
            "reset_pairing": ESP_CMD_RESET_PAIRING,
            "servo_scan": ESP_CMD_SERVO_SCAN,
            "servo_debug_enable": ESP_CMD_SERVO_DEBUG_ENABLE,
            "servo_debug_disable": ESP_CMD_SERVO_DEBUG_DISABLE,
            "servo_move": ESP_CMD_SERVO_MOVE,
        }

        def do_GET(self):
            if self.path == "/api/latest":
                payload = json.dumps(state.snapshot()).encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
                return

            if self.path == "/" or self.path == "/index.html":
                return self._send_static("index.html", "text/html")
            if self.path == "/app.js":
                return self._send_static("app.js", "application/javascript")
            if self.path == "/styles.css":
                return self._send_static("styles.css", "text/css")
            if self.path == "/ChainsawDynamics.png":
                return self._send_static("ChainsawDynamics.png", "image/png")

            self.send_response(404)
            self.end_headers()

        def do_POST(self):
            if self.path != "/api/command":
                self.send_response(404)
                self.end_headers()
                return

            content_length = int(self.headers.get("Content-Length", "0"))
            raw_body = self.rfile.read(content_length) if content_length > 0 else b"{}"
            try:
                payload = json.loads(raw_body.decode("utf-8"))
            except Exception:
                self._send_json(400, {"ok": False, "error": "invalid_json"})
                return

            command_name = str(payload.get("command", "")).strip().lower()
            value = int(payload.get("value", 0))

            command_id = self.command_map.get(command_name)
            if command_id is None:
                self._send_json(400, {"ok": False, "error": "unknown_command"})
                return

            sent = command_sender(command_id, value)
            self._send_json(200, {"ok": sent, "command": command_name})

        def log_message(self, format, *args):
            return

        def _send_json(self, status: int, payload: Dict[str, object]) -> None:
            data = json.dumps(payload).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def _send_static(self, filename: str, content_type: str):
            file_path = os.path.join(self.static_dir, filename)
            if not os.path.isfile(file_path):
                self.send_response(404)
                self.end_headers()
                return

            with open(file_path, "rb") as file_handle:
                data = file_handle.read()

            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

    return HTTPServer((bind_host, port), DashboardHandler)

import argparse
import json
import os
import socket
import struct
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

ESP_CMD_MAGIC = 0x5343
ESP_CMD_VERSION = 1
ESP_CMD_START_STREAM = 1
ESP_CMD_STOP_STREAM = 2

ESP_TLM_MAGIC = 0x5341
ESP_TLM_VERSION = 1
ESP_TLM_TYPE = 1

CMD_STRUCT = struct.Struct("<HBBI")
TLM_HEADER_STRUCT = struct.Struct("<HBB")
SNAPSHOT_STRUCT = struct.Struct("<IBBBB BBB??? 16s16s24s")

# Matches LeaderTelemetrySnapshot layout.
# Fields:
# uptimeMs, cpu0, cpu1, r0, r1,
# leaderState, followerState, mode,
# joystickPaired, calibrationDone, espNowLinked,
# leaderIp[16], followerIp[16], status[24]

latest_lock = threading.Lock()
latest_data = {
    "uptime_ms": 0,
    "cpu0_load_pct": 0,
    "cpu1_load_pct": 0,
    "leader_state": 0,
    "follower_state": 0,
    "mode": 0,
    "joystick_paired": False,
    "calibration_done": False,
    "espnow_linked": False,
    "leader_ip": "",
    "follower_ip": "",
    "status": "",
    "connected": False,
    "last_frame_ts": 0.0,
}


def decode_cstr(raw: bytes) -> str:
    return raw.split(b"\x00", 1)[0].decode("ascii", errors="ignore")


class TelemetryClient(threading.Thread):
    def __init__(self, host: str, port: int):
        super().__init__(daemon=True)
        self.host = host
        self.port = port
        self.stop_event = threading.Event()

    def run(self) -> None:
        while not self.stop_event.is_set():
            try:
                self._run_session()
            except Exception:
                self._set_connected(False)
                time.sleep(1.0)

    def _run_session(self) -> None:
        with socket.create_connection((self.host, self.port), timeout=5) as sock:
            sock.settimeout(2)
            self._set_connected(True)
            sock.sendall(CMD_STRUCT.pack(ESP_CMD_MAGIC, ESP_CMD_VERSION, ESP_CMD_START_STREAM, 0))

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
                sock.sendall(CMD_STRUCT.pack(ESP_CMD_MAGIC, ESP_CMD_VERSION, ESP_CMD_STOP_STREAM, 0))
            except Exception:
                pass

        self._set_connected(False)

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
        with latest_lock:
            latest_data["uptime_ms"] = int(fields[0])
            latest_data["cpu0_load_pct"] = int(fields[1])
            latest_data["cpu1_load_pct"] = int(fields[2])
            latest_data["leader_state"] = int(fields[5])
            latest_data["follower_state"] = int(fields[6])
            latest_data["mode"] = int(fields[7])
            latest_data["joystick_paired"] = bool(fields[8])
            latest_data["calibration_done"] = bool(fields[9])
            latest_data["espnow_linked"] = bool(fields[10])
            latest_data["leader_ip"] = decode_cstr(fields[11])
            latest_data["follower_ip"] = decode_cstr(fields[12])
            latest_data["status"] = decode_cstr(fields[13])
            latest_data["last_frame_ts"] = time.time()

    def _set_connected(self, value: bool) -> None:
        with latest_lock:
            latest_data["connected"] = value


class DashboardHandler(BaseHTTPRequestHandler):
    static_dir = os.path.join(os.path.dirname(__file__), "static")

    def do_GET(self):
        if self.path == "/api/latest":
            with latest_lock:
                payload = json.dumps(latest_data).encode("utf-8")
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

        self.send_response(404)
        self.end_headers()

    def log_message(self, format, *args):
        return

    def _send_static(self, filename: str, content_type: str):
        file_path = os.path.join(self.static_dir, filename)
        if not os.path.isfile(file_path):
            self.send_response(404)
            self.end_headers()
            return

        with open(file_path, "rb") as f:
            data = f.read()

        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


def main():
    parser = argparse.ArgumentParser(description="SoArm telemetry dashboard")
    parser.add_argument("--leader-host", default="soarm-leader.local")
    parser.add_argument("--leader-port", type=int, default=9090)
    parser.add_argument("--dashboard-port", type=int, default=8080)
    args = parser.parse_args()

    client = TelemetryClient(args.leader_host, args.leader_port)
    client.start()

    server = HTTPServer(("0.0.0.0", args.dashboard_port), DashboardHandler)
    print(f"Dashboard: http://127.0.0.1:{args.dashboard_port}")
    print(f"ESP source: {args.leader_host}:{args.leader_port}")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        client.stop_event.set()
        server.server_close()


if __name__ == "__main__":
    main()

"""Optional raw COM passthrough leader USB -> follower USB (debug only)."""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass
from typing import Optional

try:
    import serial
except ImportError:  # pragma: no cover
    serial = None  # type: ignore

from teleop_batch_codec import TELEOP_BATCH_PACKET_SIZE, drain_teleop_packets


@dataclass
class SerialBridgeStats:
    packets_forwarded: int = 0
    bytes_read: int = 0
    parse_resyncs: int = 0
    last_error: str = ""
    idle_reason: str = "stopped"
    leader_port_open: bool = False
    follower_port_open: bool = False


@dataclass
class SerialBridgeConfig:
    leader_port: str = "COM7"
    follower_port: str = "COM8"
    leader_baud: int = 1_000_000
    follower_baud: int = 115200
    read_timeout_s: float = 0.05


class SerialTeleopBridge:
    """Transparent leader COM -> follower COM forwarder (fragile: leader USB logs break framing)."""

    def __init__(self, config: SerialBridgeConfig) -> None:
        self._config = config
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._stats = SerialBridgeStats()
        self._running = False

    @property
    def running(self) -> bool:
        return self._running

    def configure(self, config: SerialBridgeConfig) -> None:
        if self._running:
            raise RuntimeError("stop the bridge before changing ports")
        self._config = config

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None
        self._running = False
        self._stats.leader_port_open = False
        self._stats.follower_port_open = False
        self._stats.idle_reason = "stopped"
        print("[com-passthrough] stopped")

    def snapshot(self) -> dict:
        return {
            "mode": "com_passthrough",
            "running": self._running,
            "leader_port": self._config.leader_port,
            "follower_port": self._config.follower_port,
            "leader_baud": self._config.leader_baud,
            "follower_baud": self._config.follower_baud,
            "packets_forwarded": self._stats.packets_forwarded,
            "bytes_read": self._stats.bytes_read,
            "parse_resyncs": self._stats.parse_resyncs,
            "last_error": self._stats.last_error,
            "idle_reason": self._stats.idle_reason,
            "leader_port_open": self._stats.leader_port_open,
            "follower_port_open": self._stats.follower_port_open,
        }

    def start(self) -> None:
        if serial is None:
            raise RuntimeError("pyserial is required: pip install pyserial")
        if self._running:
            return

        self._stop.clear()
        self._stats = SerialBridgeStats()
        self._thread = threading.Thread(target=self._run_loop, name="serial-teleop-bridge", daemon=True)
        self._thread.start()

    def _run_loop(self) -> None:
        leader = None
        follower = None
        buffer = bytearray()
        try:
            leader = serial.Serial(
                self._config.leader_port,
                self._config.leader_baud,
                timeout=self._config.read_timeout_s,
                write_timeout=1.0,
            )
            self._stats.leader_port_open = True
            print(f"[com-passthrough] OPEN OK leader={self._config.leader_port} @{self._config.leader_baud}")

            follower = serial.Serial(
                self._config.follower_port,
                self._config.follower_baud,
                timeout=self._config.read_timeout_s,
                write_timeout=1.0,
            )
            self._stats.follower_port_open = True
            print(
                f"[com-passthrough] OPEN OK follower={self._config.follower_port} "
                f"@{self._config.follower_baud}"
            )
            self._running = True
            self._stats.idle_reason = "forwarding"

            while not self._stop.is_set():
                chunk = leader.read(256)
                if chunk:
                    self._stats.bytes_read += len(chunk)
                    buffer.extend(chunk)
                    for packet in drain_teleop_packets(buffer):
                        follower.write(packet)
                        follower.flush()
                        self._stats.packets_forwarded += 1
                        if self._stats.packets_forwarded == 1:
                            print("[com-passthrough] first packet forwarded")
                else:
                    if self._stats.bytes_read == 0 and (time.monotonic() % 5.0) < 0.02:
                        self._stats.idle_reason = "no_leader_bytes"
                    time.sleep(0.001)
        except Exception as exc:
            self._stats.last_error = str(exc)
            self._stats.idle_reason = "error"
            print(f"[com-passthrough] ERROR: {exc}")
        finally:
            self._running = False
            self._stats.leader_port_open = False
            self._stats.follower_port_open = False
            for port in (leader, follower):
                if port is not None and port.is_open:
                    port.close()

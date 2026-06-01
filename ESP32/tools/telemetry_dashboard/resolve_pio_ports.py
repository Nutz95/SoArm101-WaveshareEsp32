"""Read leader/follower USB COM ports and baud from ESP32/platformio.ini."""

from __future__ import annotations

import configparser
import json
import re
import sys
from pathlib import Path


def _repo_esp32_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _parse_build_flag_value(block: str, key: str) -> str | None:
    pattern = re.compile(rf"-D{re.escape(key)}=(\d+)")
    match = pattern.search(block)
    if not match:
        return None
    return match.group(1)


def resolve_ports(ini_path: Path | None = None) -> dict[str, str | int]:
    ini_path = ini_path or (_repo_esp32_root() / "platformio.ini")
    parser = configparser.ConfigParser()
    parser.read(ini_path, encoding="utf-8")

    leader_port = parser.get("env:leader", "monitor_port", fallback="").strip()
    follower_port = parser.get("env:follower", "monitor_port", fallback="").strip()
    leader_baud = int(parser.get("env:leader", "monitor_speed", fallback="115200"))
    follower_baud = int(parser.get("env:follower", "monitor_speed", fallback="115200"))

    leader_flags = parser.get("env:leader", "build_flags", fallback="")
    usb_baud = _parse_build_flag_value(leader_flags, "USB_CDC_BAUD")
    if usb_baud is not None:
        leader_baud = int(usb_baud)

    return {
        "leader_port": leader_port,
        "follower_port": follower_port,
        "leader_baud": leader_baud,
        "follower_baud": follower_baud,
    }


def main() -> int:
    data = resolve_ports()
    json.dump(data, sys.stdout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

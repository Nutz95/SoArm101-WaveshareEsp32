import json
import re
from pathlib import Path
from typing import Any, Dict, List, Optional


SERVO_TELEMETRY_RE = re.compile(
    r"#(?P<id>\d+)\s+p(?P<position>-?\d+)(?:\s+v(?P<voltage>-?\d+)\s+t(?P<temperature>-?\d+)\s+m(?P<mode>-?\d+))?"
)


def _clamp_speed_pct(value: Any) -> int:
    try:
        numeric = int(value)
    except (TypeError, ValueError):
        numeric = 35
    return max(0, min(100, numeric))


def _as_bool(value: Any, default: bool) -> bool:
    if isinstance(value, bool):
        return value
    if value is None:
        return default
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "on"}
    return bool(value)


def _clamp_transport_mode(value: Any) -> int:
    try:
        numeric = int(value)
    except (TypeError, ValueError):
        numeric = 0
    return 1 if numeric == 1 else 0


def parse_servo_ids(raw_ids: str) -> List[int]:
    if not raw_ids or raw_ids == "-":
        return []

    ids: List[int] = []
    for token in re.split(r"[ ,;|]+", raw_ids):
        token = token.strip()
        if not token:
            continue
        try:
            ids.append(int(token))
        except ValueError:
            continue
    return ids


def parse_servo_telemetry(raw_text: str) -> Dict[int, Dict[str, int]]:
    if not raw_text or raw_text == "-":
        return {}

    parsed: Dict[int, Dict[str, int]] = {}
    for match in SERVO_TELEMETRY_RE.finditer(raw_text):
        servo_id = int(match.group("id"))
        voltage = match.group("voltage")
        temperature = match.group("temperature")
        mode = match.group("mode")
        parsed[servo_id] = {
            "position": int(match.group("position")),
            "voltage": int(voltage) if voltage is not None else 0,
            "temperature": int(temperature) if temperature is not None else 0,
            "mode": int(mode) if mode is not None else 0,
        }
    return parsed


class TeleopConfigStore:
    def __init__(self, file_path: Path):
        self._file_path = file_path
        self._config = self._load()

    def snapshot(self) -> Dict[str, Any]:
        return dict(self._config)

    def update(self, payload: Dict[str, Any]) -> Dict[str, Any]:
        self._config["enabled"] = _as_bool(payload.get("enabled"), self._config["enabled"])
        self._config["same_id_mapping"] = _as_bool(payload.get("same_id_mapping"), self._config["same_id_mapping"])
        self._config["calibration_required"] = _as_bool(payload.get("calibration_required"), self._config["calibration_required"])
        self._config["speed_pct"] = _clamp_speed_pct(payload.get("speed_pct", self._config["speed_pct"]))
        self._config["transport_mode"] = _clamp_transport_mode(payload.get("transport_mode", self._config.get("transport_mode", 0)))
        self._save()
        return self.snapshot()

    def _load(self) -> Dict[str, Any]:
        defaults = {
            "enabled": True,
            "same_id_mapping": True,
            "calibration_required": False,
            "speed_pct": 35,
            "transport_mode": 0,
        }

        if not self._file_path.is_file():
            return defaults

        try:
            loaded = json.loads(self._file_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return defaults

        defaults.update(
            {
                "enabled": _as_bool(loaded.get("enabled"), defaults["enabled"]),
                "same_id_mapping": _as_bool(loaded.get("same_id_mapping"), defaults["same_id_mapping"]),
                "calibration_required": _as_bool(loaded.get("calibration_required"), defaults["calibration_required"]),
                "speed_pct": _clamp_speed_pct(loaded.get("speed_pct")),
                "transport_mode": _clamp_transport_mode(loaded.get("transport_mode", defaults["transport_mode"])),
            }
        )
        return defaults

    def _save(self) -> None:
        self._file_path.write_text(json.dumps(self._config, indent=2), encoding="utf-8")


def build_teleop_state(snapshot: Dict[str, Any], config: Dict[str, Any]) -> Dict[str, Any]:
    leader_ids = parse_servo_ids(str(snapshot.get("leader_servo_ids", "")))
    follower_ids = parse_servo_ids(str(snapshot.get("follower_servo_ids", "")))
    leader_telemetry = parse_servo_telemetry(str(snapshot.get("leader_servo_telemetry", "")))
    follower_telemetry = parse_servo_telemetry(str(snapshot.get("follower_servo_telemetry", "")))

    ordered_ids: List[int] = []
    seen = set()
    for servo_id in leader_ids + follower_ids:
        if servo_id in seen:
            continue
        seen.add(servo_id)
        ordered_ids.append(servo_id)

    cards: List[Dict[str, Any]] = []
    matched_count = 0
    mirrorable_count = 0
    continuous_enabled = bool(snapshot.get("teleop_continuous_enabled", False))
    continuous_servo_id = int(snapshot.get("teleop_continuous_servo_id", 0) or 0)

    for servo_id in ordered_ids:
        leader_data = leader_telemetry.get(servo_id)
        follower_data = follower_telemetry.get(servo_id)
        leader_present = servo_id in leader_ids
        follower_present = servo_id in follower_ids
        mapped = bool(config.get("same_id_mapping", True)) and leader_present and follower_present
        if mapped:
            matched_count += 1

        can_mirror = (
            bool(config.get("enabled", True))
            and mapped
            and leader_data is not None
            and snapshot.get("espnow_linked", False)
        )
        if can_mirror:
            mirrorable_count += 1

        active_continuous = bool(continuous_enabled and (continuous_servo_id == 0 or continuous_servo_id == servo_id))

        leader_position = leader_data.get("position") if leader_data else None
        follower_position = follower_data.get("position") if follower_data else None
        delta = None
        if leader_position is not None and follower_position is not None:
            delta = leader_position - follower_position

        cards.append(
            {
                "servo_id": servo_id,
                "leader_present": leader_present,
                "follower_present": follower_present,
                "mapped": mapped,
                "can_mirror": can_mirror,
                "leader": leader_data,
                "follower": follower_data,
                "position_delta": delta,
                "active_continuous": active_continuous,
            }
        )

    return {
        "config": dict(config),
        "runtime": {
            "continuous_enabled": continuous_enabled,
            "continuous_servo_id": continuous_servo_id,
            "transport_mode": int(snapshot.get("teleop_transport_mode", config.get("transport_mode", 0)) or 0),
            "fw_latency_last_ms": int(snapshot.get("teleop_mirror_latency_last_ms", 0) or 0),
            "fw_latency_ewma_ms": int(snapshot.get("teleop_mirror_latency_ewma_ms", 0) or 0),
            "fw_latency_p95_ms": int(snapshot.get("teleop_mirror_latency_p95_ms", 0) or 0),
            "fw_pending_count": int(snapshot.get("teleop_mirror_pending_count", 0) or 0),
            "fw_timeout_count": int(snapshot.get("teleop_mirror_timeout_count", 0) or 0),
        },
        "summary": {
            "leader_detected": len(leader_ids),
            "follower_detected": len(follower_ids),
            "matched": matched_count,
            "mirrorable": mirrorable_count,
        },
        "cards": cards,
    }


def build_mirror_values(snapshot: Dict[str, Any], config: Dict[str, Any], servo_id: Optional[int] = None) -> List[int]:
    teleop_state = build_teleop_state(snapshot, config)
    speed_pct = int(config.get("speed_pct", 35)) & 0xFF

    packed_values: List[int] = []
    for card in teleop_state["cards"]:
        card_servo_id = int(card["servo_id"])
        if servo_id is not None and card_servo_id != servo_id:
            continue
        if not card.get("can_mirror"):
            continue

        leader = card.get("leader") or {}
        position = leader.get("position")
        if position is None:
            continue

        packed = (card_servo_id & 0xFF) | ((int(position) & 0xFFFF) << 8) | ((speed_pct & 0xFF) << 24)
        packed_values.append(packed)

    return packed_values
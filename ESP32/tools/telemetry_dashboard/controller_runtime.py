import json
from pathlib import Path
from typing import Any, Dict


DEFAULT_CONTROLLER_NAME = "Xbox Wireless Controller"
ALLOWED_BUTTON_ACTIONS = {
    "view": "View button",
    "menu": "Menu button",
    "share": "Share button",
    "left_stick": "Left stick press",
    "right_stick": "Right stick press",
    "none": "Disabled",
}


def _as_bool(value: Any, default: bool) -> bool:
    if isinstance(value, bool):
        return value
    if value is None:
        return default
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "on"}
    return bool(value)


def _clamp_percent(value: Any, default: int) -> int:
    try:
        numeric = int(value)
    except (TypeError, ValueError):
        numeric = default
    return max(0, min(100, numeric))


def _sanitize_name(value: Any, default: str) -> str:
    text = str(value or "").strip()
    return text[:64] if text else default


def _sanitize_button_action(value: Any, default: str) -> str:
    key = str(value or "").strip().lower()
    return key if key in ALLOWED_BUTTON_ACTIONS else default


class ControllerConfigStore:
    def __init__(self, file_path: Path):
        self._file_path = file_path
        self._config = self._load()

    def snapshot(self) -> Dict[str, Any]:
        return dict(self._config)

    def update(self, payload: Dict[str, Any]) -> Dict[str, Any]:
        self._config["enabled"] = _as_bool(payload.get("enabled"), self._config["enabled"])
        self._config["preferred_controller_name"] = _sanitize_name(
            payload.get("preferred_controller_name"), self._config["preferred_controller_name"]
        )
        self._config["auto_reconnect"] = _as_bool(payload.get("auto_reconnect"), self._config["auto_reconnect"])
        self._config["deadzone_pct"] = _clamp_percent(payload.get("deadzone_pct"), self._config["deadzone_pct"])
        self._config["trigger_threshold_pct"] = _clamp_percent(
            payload.get("trigger_threshold_pct"), self._config["trigger_threshold_pct"]
        )
        self._config["invert_left_y"] = _as_bool(payload.get("invert_left_y"), self._config["invert_left_y"])
        self._config["invert_right_y"] = _as_bool(payload.get("invert_right_y"), self._config["invert_right_y"])
        self._config["mode_cycle_button"] = _sanitize_button_action(
            payload.get("mode_cycle_button"), self._config["mode_cycle_button"]
        )
        self._config["user_action_button"] = _sanitize_button_action(
            payload.get("user_action_button"), self._config["user_action_button"]
        )
        self._save()
        return self.snapshot()

    def state(self) -> Dict[str, Any]:
        config = self.snapshot()
        return {
            "config": config,
            "summary": {
                "status": "ready" if config["enabled"] else "disabled",
                "controller_label": config["preferred_controller_name"],
                "mode_cycle_label": ALLOWED_BUTTON_ACTIONS[config["mode_cycle_button"]],
                "user_action_label": ALLOWED_BUTTON_ACTIONS[config["user_action_button"]],
            },
        }

    def _load(self) -> Dict[str, Any]:
        defaults = {
            "enabled": False,
            "preferred_controller_name": DEFAULT_CONTROLLER_NAME,
            "auto_reconnect": True,
            "deadzone_pct": 12,
            "trigger_threshold_pct": 35,
            "invert_left_y": True,
            "invert_right_y": True,
            "mode_cycle_button": "view",
            "user_action_button": "share",
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
                "preferred_controller_name": _sanitize_name(
                    loaded.get("preferred_controller_name"), defaults["preferred_controller_name"]
                ),
                "auto_reconnect": _as_bool(loaded.get("auto_reconnect"), defaults["auto_reconnect"]),
                "deadzone_pct": _clamp_percent(loaded.get("deadzone_pct"), defaults["deadzone_pct"]),
                "trigger_threshold_pct": _clamp_percent(
                    loaded.get("trigger_threshold_pct"), defaults["trigger_threshold_pct"]
                ),
                "invert_left_y": _as_bool(loaded.get("invert_left_y"), defaults["invert_left_y"]),
                "invert_right_y": _as_bool(loaded.get("invert_right_y"), defaults["invert_right_y"]),
                "mode_cycle_button": _sanitize_button_action(
                    loaded.get("mode_cycle_button"), defaults["mode_cycle_button"]
                ),
                "user_action_button": _sanitize_button_action(
                    loaded.get("user_action_button"), defaults["user_action_button"]
                ),
            }
        )
        return defaults

    def _save(self) -> None:
        self._file_path.write_text(json.dumps(self._config, indent=2), encoding="utf-8")

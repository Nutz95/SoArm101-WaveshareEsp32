import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
TELEMETRY_DASHBOARD_DIR = PROJECT_ROOT / "tools" / "telemetry_dashboard"
if str(TELEMETRY_DASHBOARD_DIR) not in sys.path:
    sys.path.insert(0, str(TELEMETRY_DASHBOARD_DIR))

from teleop_runtime import build_mirror_values, build_teleop_state  # noqa: E402


class TeleopRuntimeTest(unittest.TestCase):
    def _base_snapshot(self):
        return {
            "connected": False,
            "espnow_linked": True,
            "leader_servo_ids": "1,2",
            "follower_servo_ids": "1",
            "leader_servo_telemetry": "#1 p1000;#2 p2000;",
            "follower_servo_telemetry": "#1 p980;",
            "teleop_continuous_enabled": False,
            "teleop_continuous_servo_id": 0,
        }

    def _base_config(self):
        return {
            "enabled": True,
            "same_id_mapping": True,
            "calibration_required": False,
            "speed_pct": 35,
        }

    def test_mirrorable_uses_espnow_link_only(self):
        snapshot = self._base_snapshot()
        config = self._base_config()

        state = build_teleop_state(snapshot, config)

        self.assertEqual(1, state["summary"]["mirrorable"])

    def test_mirror_values_follow_same_id_mapping(self):
        snapshot = self._base_snapshot()
        config = self._base_config()

        values = build_mirror_values(snapshot, config)

        self.assertEqual(1, len(values))
        packed = values[0]
        self.assertEqual(1, packed & 0xFF)

    def test_start_stop_continuous_runtime_reflects_snapshot(self):
        snapshot = self._base_snapshot()
        config = self._base_config()

        snapshot["teleop_continuous_enabled"] = True
        snapshot["teleop_continuous_servo_id"] = 0
        started = build_teleop_state(snapshot, config)
        self.assertTrue(started["runtime"]["continuous_enabled"])
        self.assertEqual(0, started["runtime"]["continuous_servo_id"])

        snapshot["teleop_continuous_enabled"] = False
        snapshot["teleop_continuous_servo_id"] = 0
        stopped = build_teleop_state(snapshot, config)
        self.assertFalse(stopped["runtime"]["continuous_enabled"])

    def test_stability_ids_with_transient_empty_follower_ids(self):
        config = self._base_config()
        snapshot = self._base_snapshot()

        stable = build_teleop_state(snapshot, config)
        self.assertEqual(1, stable["summary"]["mirrorable"])

        snapshot["follower_servo_ids"] = ""
        transient = build_teleop_state(snapshot, config)
        self.assertEqual(0, transient["summary"]["mirrorable"])
        self.assertEqual(2, len(transient["cards"]))


if __name__ == "__main__":
    unittest.main()

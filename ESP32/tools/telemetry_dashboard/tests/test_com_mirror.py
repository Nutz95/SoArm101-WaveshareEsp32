from teleop_batch_codec import TELEOP_BATCH_PACKET_SIZE, pack_teleop_batch
from telemetry_com_mirror import build_mirror_batch_entries


def test_pack_teleop_batch_size() -> None:
    packet = pack_teleop_batch([(1, 2048), (2, 1500)], 100, 42)
    assert len(packet) == TELEOP_BATCH_PACKET_SIZE


def test_build_mirror_batch_entries() -> None:
    snapshot = {
        "leader_servo_ids": "1,2,3",
        "follower_servo_ids": "1,2,3",
        "leader_servo_telemetry": "#1 p2048 v120 t30 m0;#2 p1500 v120 t30 m0",
        "leader_calibration_min": [100, 100, 100, 0, 0, 0],
        "leader_calibration_max": [3900, 3900, 3900, 4095, 4095, 4095],
        "follower_calibration_min": [100, 100, 100, 0, 0, 0],
        "follower_calibration_max": [3900, 3900, 3900, 4095, 4095, 4095],
    }
    entries = build_mirror_batch_entries(snapshot, 100)
    assert len(entries) >= 2
    assert entries[0][0] == 1

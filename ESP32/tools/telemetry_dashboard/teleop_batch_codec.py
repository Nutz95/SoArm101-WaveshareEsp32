"""Binary teleop batch frames (must match ESP32 teleop_batch::BatchPacket)."""

from __future__ import annotations

import struct
from typing import List, Sequence, Tuple

TELEOP_BATCH_MAGIC = 0x5457
TELEOP_BATCH_VERSION = 2
TELEOP_BATCH_TYPE = 1
TELEOP_BATCH_PACKET_SIZE = 27
TELEOP_BATCH_HEADER = struct.Struct("<HBBBHBB")
TELEOP_BATCH_ENTRY = struct.Struct("<Bh")
TELEOP_BATCH_MAX_SERVOS = 6


def pack_teleop_batch(
    entries: Sequence[Tuple[int, int]],
    speed_percent: int,
    request_id: int,
    flags: int = 0,
) -> bytes:
    count = min(len(entries), TELEOP_BATCH_MAX_SERVOS)
    payload = bytearray(TELEOP_BATCH_MAX_SERVOS * TELEOP_BATCH_ENTRY.size)
    for index in range(count):
        servo_id, position = entries[index]
        TELEOP_BATCH_ENTRY.pack_into(payload, index * TELEOP_BATCH_ENTRY.size, servo_id & 0xFF, int(position))

    header = TELEOP_BATCH_HEADER.pack(
        TELEOP_BATCH_MAGIC,
        TELEOP_BATCH_VERSION,
        flags & 0xFF,
        TELEOP_BATCH_TYPE,
        request_id & 0xFFFF,
        count & 0xFF,
        max(0, min(100, int(speed_percent))) & 0xFF,
    )
    return header + bytes(payload)


def drain_teleop_packets(buffer: bytearray) -> List[bytes]:
    packets: List[bytes] = []
    offset = 0
    resyncs = 0
    while offset + TELEOP_BATCH_PACKET_SIZE <= len(buffer):
        magic = buffer[offset] | (buffer[offset + 1] << 8)
        if magic != TELEOP_BATCH_MAGIC:
            offset += 1
            resyncs += 1
            continue
        packets.append(bytes(buffer[offset : offset + TELEOP_BATCH_PACKET_SIZE]))
        offset += TELEOP_BATCH_PACKET_SIZE
    if offset > 0:
        del buffer[:offset]
    if len(buffer) > 512:
        del buffer[:-4]
        resyncs += 1
    return packets

#include "leader_telemetry_serializer.h"

namespace soarm {

LeaderTelemetrySerializer::Packet LeaderTelemetrySerializer::serialize(
    const LeaderTelemetrySnapshot &snapshot) const {
  Packet packet{};
  packet.magic = kMagic;
  packet.version = kVersion;
  packet.packetType = kTelemetryType;
  packet.payload = snapshot;
  return packet;
}

} // namespace soarm

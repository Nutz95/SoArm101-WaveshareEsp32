#include "presence_message_type_name.h"

namespace soarm {

namespace {

constexpr const char kUnknown[] = "Unknown";

struct PresenceMessageTypeNameEntry {
  PresenceMessageType type;
  const char *name;
};

constexpr PresenceMessageTypeNameEntry kPresenceMessageTypeNames[] = {
    {PresenceMessageType::PairRequest, "PairRequest"},
    {PresenceMessageType::PairAck, "PairAck"},
    {PresenceMessageType::Presence, "Presence"},
    {PresenceMessageType::PairReset, "PairReset"},
    {PresenceMessageType::ServoScan, "ServoScan"},
    {PresenceMessageType::ServoControl, "ServoControl"},
    {PresenceMessageType::ServoCommandAck, "ServoCommandAck"},
    {PresenceMessageType::ServoControlBatch, "ServoControlBatch"},
    {PresenceMessageType::LinkHeartbeat, "LinkHeartbeat"},
    {PresenceMessageType::WifiDirectOffer, "WifiDirectOffer"},
    {PresenceMessageType::WifiDirectAck, "WifiDirectAck"},
    {PresenceMessageType::TeleopMirrorCompact, "TeleopMirrorCompact"},
};

const char *lookupPresenceMessageTypeName(uint8_t raw) {
  for (const PresenceMessageTypeNameEntry &entry : kPresenceMessageTypeNames) {
    if (static_cast<uint8_t>(entry.type) == raw) {
      return entry.name;
    }
  }
  return kUnknown;
}

} // namespace

const char *presenceMessageTypeName(PresenceMessageType type) {
  return lookupPresenceMessageTypeName(static_cast<uint8_t>(type));
}

const char *presenceMessageTypeNameRaw(uint8_t raw) {
  return lookupPresenceMessageTypeName(raw);
}

} // namespace soarm

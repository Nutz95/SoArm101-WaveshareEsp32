#include "presence_message_type_name.h"

namespace soarm {

const char *presenceMessageTypeName(PresenceMessageType type) {
  switch (type) {
  case PresenceMessageType::PairRequest:
    return "PairRequest";
  case PresenceMessageType::PairAck:
    return "PairAck";
  case PresenceMessageType::Presence:
    return "Presence";
  case PresenceMessageType::PairReset:
    return "PairReset";
  case PresenceMessageType::ServoScan:
    return "ServoScan";
  case PresenceMessageType::ServoControl:
    return "ServoControl";
  case PresenceMessageType::ServoCommandAck:
    return "ServoCommandAck";
  case PresenceMessageType::ServoControlBatch:
    return "ServoControlBatch";
  case PresenceMessageType::LinkHeartbeat:
    return "LinkHeartbeat";
  default:
    return "Unknown";
  }
}

const char *presenceMessageTypeNameRaw(uint8_t raw) {
  return presenceMessageTypeName(static_cast<PresenceMessageType>(raw));
}

} // namespace soarm

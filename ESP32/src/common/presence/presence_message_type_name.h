#pragma once

#include "presence_message_type.h"

namespace soarm {

const char *presenceMessageTypeName(PresenceMessageType type);
const char *presenceMessageTypeNameRaw(uint8_t raw);

} // namespace soarm

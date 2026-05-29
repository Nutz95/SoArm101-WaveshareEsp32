#pragma once

#include <cstddef>

namespace soarm {

// Resolve follower IP for Wi-Fi teleop: ESP-NOW presence IP first, then mDNS.
bool resolveFollowerEndpoint(const char *presenceIp, char *out, size_t outLen);

} // namespace soarm

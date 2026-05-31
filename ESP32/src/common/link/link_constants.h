#pragma once

#include <cstdint>

namespace soarm {
namespace link {

// Minimum interval between outbound link frames when the peer is already talking.
constexpr uint32_t kHeartbeatIntervalMs = 1000U;
// Full presence (telemetry + ACK fields) when idle, not more often than this.
constexpr uint32_t kFullPresenceIntervalMs = 5000U;
// Peer considered offline if no inbound ESP-NOW activity for this long.
constexpr uint32_t kPeerAliveTimeoutMs = 12000U;

} // namespace link
} // namespace soarm

#pragma once

#include "link_constants.h"

#include <cstdint>

namespace soarm {

// Tracks bidirectional link liveness. Any inbound/outbound peer frame resets the alive timer.
class LinkHeartbeatManager {
public:
  void reset() {
    lastPeerActivityMs_ = 0U;
    lastOutboundMs_ = 0U;
    lastFullPresenceMs_ = 0U;
  }

  void notifyPeerActivity(uint32_t nowMs) {
    lastPeerActivityMs_ = nowMs;
  }

  void markOutboundSent(uint32_t nowMs) {
    lastOutboundMs_ = nowMs;
    lastPeerActivityMs_ = nowMs;
  }

  void markFullPresenceSent(uint32_t nowMs) {
    lastFullPresenceMs_ = nowMs;
    markOutboundSent(nowMs);
  }

  bool isPeerAlive(uint32_t nowMs, uint32_t timeoutMs = link::kPeerAliveTimeoutMs) const {
    if (lastPeerActivityMs_ == 0U) {
      return false;
    }
    return (nowMs - lastPeerActivityMs_) <= timeoutMs;
  }

  bool shouldSendHeartbeat(uint32_t nowMs, uint32_t intervalMs = link::kHeartbeatIntervalMs) const {
    if (lastOutboundMs_ == 0U) {
      return true;
    }
    return (nowMs - lastOutboundMs_) >= intervalMs;
  }

  bool shouldSendFullPresence(uint32_t nowMs, uint32_t intervalMs = link::kFullPresenceIntervalMs) const {
    if (lastFullPresenceMs_ == 0U) {
      return true;
    }
    return (nowMs - lastFullPresenceMs_) >= intervalMs;
  }

  uint32_t lastPeerActivityMs() const {
    return lastPeerActivityMs_;
  }

private:
  uint32_t lastPeerActivityMs_{0U};
  uint32_t lastOutboundMs_{0U};
  uint32_t lastFullPresenceMs_{0U};
};

} // namespace soarm

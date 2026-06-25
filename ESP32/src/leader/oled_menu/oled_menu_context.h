#pragma once

#include <cstdint>

namespace soarm {

// Snapshot passed to menu screens for read-only drawing.
struct OledMenuContext {
  const char *leaderIp{nullptr};
  const char *followerIpHint{nullptr};
  const char *pairedPeerMac{nullptr};
  bool espNowLinked{false};
  bool espNowPaired{false};
  bool xboxBlePaired{false};
  bool telemetryListening{false};
  uint8_t leaderServoCount{0U};
  uint8_t followerServoCount{0U};
};

} // namespace soarm

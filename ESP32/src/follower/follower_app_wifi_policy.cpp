#include "follower_app.h"

namespace soarm {

void FollowerApp::syncWifiRadioPolicy(uint32_t nowMs) {
  (void)nowMs;
  // Keep STA up for OTA; do not disconnect (same ESP-NOW stability issue as leader).
  wifiOta_.setStaConnectDesired(true);
}

} // namespace soarm

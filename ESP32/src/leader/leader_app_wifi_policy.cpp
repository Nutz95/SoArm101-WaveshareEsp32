#include "leader_app.h"

#include "../common/controller/controller_operation_profile.h"

namespace soarm {

void LeaderApp::syncWifiRadioPolicyForProfile(ControllerOperationProfile profile) {
  // Phase 2: pause :9090 telemetry during ESP-NOW teleop (STA stays up for OTA + BLE coexistence).
  const bool pauseDashboardStream = (profile == ControllerOperationProfile::TeleopEspNow);
  telemetryStreamServer_.setListeningEnabled(!pauseDashboardStream);

  // Do not call WiFi.disconnect() — panics with ESP-NOW + NimBLE. USB debug channel = Phase 3.
  wifiOta_.setStaConnectDesired(true);
}

} // namespace soarm

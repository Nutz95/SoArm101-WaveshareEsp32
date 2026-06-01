#pragma once

#include <cstdint>

namespace soarm {

// Soft-AP / STA session for leader-follower direct Wi-Fi (separate from home-router OTA STA).
class WifiDirectRadioService {
public:
  bool beginAccessPoint(const char *ssid, const char *password, uint8_t channel);
  bool beginStation(const char *ssid, const char *password);
  void endSession(bool disconnectStation);
  void tickStationLink();

  bool isAccessPointActive() const;
  bool isStationActive() const;
  const char *accessPointIp() const;
  const char *stationIp() const;

private:
  bool accessPointActive_{false};
  bool stationActive_{false};
  char accessPointIpBuf_[16]{};
  char stationIpBuf_[16]{};
};

} // namespace soarm

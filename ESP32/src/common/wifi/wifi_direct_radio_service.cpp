#include "wifi_direct_radio_service.h"

#include "../usb_debug_log_gate.h"

#include <WiFi.h>
#include <cstring>

namespace soarm {

bool WifiDirectRadioService::beginAccessPoint(const char *ssid, const char *password, uint8_t channel) {
  if (ssid == nullptr || password == nullptr || ssid[0] == '\0' || password[0] == '\0') {
    return false;
  }

  endSession(false);
  WiFi.mode(WIFI_AP_STA);
#if defined(LEADER_ENABLE_XBOX_BLE) && LEADER_ENABLE_XBOX_BLE
  WiFi.setSleep(true);
#else
  WiFi.setSleep(false);
#endif

  const uint8_t safeChannel = channel == 0U ? 1U : channel;
  const bool started = WiFi.softAP(ssid, password, safeChannel, 0, 4);
  if (!started) {
    USB_DEBUG_LOGLN("[WIFI-DIRECT] softAP start failed");
    return false;
  }

  accessPointActive_ = true;
  stationActive_ = false;
  const String apIp = WiFi.softAPIP().toString();
  strncpy(accessPointIpBuf_, apIp.c_str(), sizeof(accessPointIpBuf_) - 1U);
  accessPointIpBuf_[sizeof(accessPointIpBuf_) - 1U] = '\0';
  USB_DEBUG_LOGF("[WIFI-DIRECT] AP up ssid=%s ip=%s ch=%u\n", ssid, accessPointIpBuf_, safeChannel);
  return true;
}

bool WifiDirectRadioService::beginStation(const char *ssid, const char *password) {
  if (ssid == nullptr || password == nullptr || ssid[0] == '\0' || password[0] == '\0') {
    return false;
  }

  endSession(false);
  WiFi.mode(WIFI_STA);
#if defined(LEADER_ENABLE_XBOX_BLE) && LEADER_ENABLE_XBOX_BLE
  WiFi.setSleep(true);
#else
  WiFi.setSleep(false);
#endif
  WiFi.begin(ssid, password);
  stationActive_ = true;
  accessPointActive_ = false;
  stationIpBuf_[0] = '\0';
  USB_DEBUG_LOGF("[WIFI-DIRECT] STA joining ssid=%s\n", ssid);
  return true;
}

void WifiDirectRadioService::endSession(bool disconnectStation) {
  if (accessPointActive_) {
    WiFi.softAPdisconnect(true);
    accessPointActive_ = false;
    accessPointIpBuf_[0] = '\0';
    USB_DEBUG_LOGLN("[WIFI-DIRECT] AP stopped");
  }

  if (stationActive_ && disconnectStation) {
    WiFi.disconnect(false, true);
    stationActive_ = false;
    stationIpBuf_[0] = '\0';
    USB_DEBUG_LOGLN("[WIFI-DIRECT] STA disconnected");
  } else if (stationActive_) {
    stationActive_ = false;
    stationIpBuf_[0] = '\0';
  }
}

void WifiDirectRadioService::tickStationLink() {
  if (!stationActive_ || WiFi.status() != WL_CONNECTED) {
    return;
  }

  const String ip = WiFi.localIP().toString();
  if (ip.length() == 0U) {
    return;
  }

  if (strncmp(stationIpBuf_, ip.c_str(), sizeof(stationIpBuf_)) != 0) {
    strncpy(stationIpBuf_, ip.c_str(), sizeof(stationIpBuf_) - 1U);
    stationIpBuf_[sizeof(stationIpBuf_) - 1U] = '\0';
    USB_DEBUG_LOGF("[WIFI-DIRECT] STA ip=%s\n", stationIpBuf_);
  }
}

bool WifiDirectRadioService::isAccessPointActive() const {
  return accessPointActive_;
}

bool WifiDirectRadioService::isStationActive() const {
  return stationActive_;
}

const char *WifiDirectRadioService::accessPointIp() const {
  return accessPointIpBuf_;
}

const char *WifiDirectRadioService::stationIp() const {
  return stationIpBuf_;
}

} // namespace soarm

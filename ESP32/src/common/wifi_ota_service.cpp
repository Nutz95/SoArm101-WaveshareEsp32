#include "wifi_ota_service.h"

#include "usb_debug_log_gate.h"

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <cstring>

namespace soarm {

WifiOtaService::WifiOtaService(const char *ssid,
                               const char *password,
                               const char *hostname)
    : ssid_(ssid), password_(password), hostname_(hostname) {}

void WifiOtaService::begin(WifiOtaCallbacks callbacks) {
  callbacks_ = callbacks;

  WiFi.mode(WIFI_STA);
#if defined(LEADER_ENABLE_XBOX_BLE) && LEADER_ENABLE_XBOX_BLE
  WiFi.setSleep(true);
#else
  WiFi.setSleep(false);
#endif
  WiFi.setHostname(hostname_);

  const bool ssidConfigured = (ssid_ != nullptr && ssid_[0] != '\0');
  if (!ssidConfigured) {
    staConnectDesired_ = false;
    USB_DEBUG_LOGLN("[WiFi] SSID not configured (set SOARM_WIFI_SSID at build time)");
  } else if (staConnectDesired_) {
    WiFi.begin(ssid_, password_);
  }

  ArduinoOTA.setHostname(hostname_);

  ArduinoOTA.onStart([this]() {
    otaInProgress_ = true;
    if (!staConnectDesired_) {
      staConnectDesired_ = true;
    }
    if (ssid_ != nullptr && ssid_[0] != '\0' && WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid_, password_);
    }
    if (callbacks_.onOtaBegin) {
      callbacks_.onOtaBegin();
    }
  });

  ArduinoOTA.onEnd([this]() {
    otaInProgress_ = false;
    if (callbacks_.onOtaEnd) {
      callbacks_.onOtaEnd();
    }
  });

  ArduinoOTA.onError([this](ota_error_t error) {
    otaInProgress_ = false;
    if (callbacks_.onOtaError) {
      callbacks_.onOtaError(static_cast<uint32_t>(error));
    }
  });

  ArduinoOTA.begin();
}

void WifiOtaService::setStaConnectDesired(bool desired) {
  if (staConnectDesired_ == desired) {
    return;
  }

  staConnectDesired_ = desired;

  if (!desired) {
    suspendHomeStation();
  } else if (ssid_ != nullptr && ssid_[0] != '\0') {
    if (WiFi.getMode() != WIFI_STA) {
      WiFi.mode(WIFI_STA);
    }
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid_, password_);
    }
  }
}

bool WifiOtaService::isStaConnectDesired() const {
  return staConnectDesired_;
}

void WifiOtaService::tick() {
  ArduinoOTA.handle();

  const bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected) {
    const String liveIp = WiFi.localIP().toString();
    if (liveIp.length() > 0U && liveIp != "0.0.0.0") {
      strncpy(ipBuf_, liveIp.c_str(), sizeof(ipBuf_) - 1);
      ipBuf_[sizeof(ipBuf_) - 1] = '\0';
    }
  }

  if (!staConnectDesired_ && !otaInProgress_) {
    return;
  }

  if (connected && !wasConnected_) {
    wasConnected_ = true;
    const String ip = WiFi.localIP().toString();
    strncpy(ipBuf_, ip.c_str(), sizeof(ipBuf_) - 1);
    ipBuf_[sizeof(ipBuf_) - 1] = '\0';
    if (callbacks_.onWifiConnected) {
      callbacks_.onWifiConnected(ipBuf_);
    }
  } else if (!connected && wasConnected_) {
    wasConnected_ = false;
    ipBuf_[0] = '\0';
    if (callbacks_.onWifiDisconnected) {
      callbacks_.onWifiDisconnected();
    }
  }
}

bool WifiOtaService::isConnected() const {
  return wasConnected_;
}

bool WifiOtaService::isOtaInProgress() const {
  return otaInProgress_;
}

const char *WifiOtaService::ipAddress() const {
  if (WiFi.status() == WL_CONNECTED) {
    const String liveIp = WiFi.localIP().toString();
    if (liveIp.length() > 0U && liveIp != "0.0.0.0") {
      strncpy(ipBuf_, liveIp.c_str(), sizeof(ipBuf_) - 1);
      ipBuf_[sizeof(ipBuf_) - 1] = '\0';
    }
  }
  return ipBuf_;
}

void WifiOtaService::suspendHomeStation() {
  if (otaInProgress_) {
    return;
  }

  WiFi.disconnect(false, false);
  if (wasConnected_) {
    wasConnected_ = false;
    if (callbacks_.onWifiDisconnected) {
      callbacks_.onWifiDisconnected();
    }
  }
  ipBuf_[0] = '\0';
  USB_DEBUG_LOGLN("[WiFi] home STA suspended");
}

void WifiOtaService::restoreHomeStation() {
  WiFi.softAPdisconnect(true);
  // Do not call WiFi.disconnect(true, true): deinits ESP-NOW and can panic with NimBLE.
  WiFi.mode(WIFI_STA);
#if defined(LEADER_ENABLE_XBOX_BLE) && LEADER_ENABLE_XBOX_BLE
  WiFi.setSleep(true);
#else
  WiFi.setSleep(false);
#endif
  wasConnected_ = false;
  ipBuf_[0] = '\0';

  staConnectDesired_ = true;
  if (ssid_ != nullptr && ssid_[0] != '\0') {
    WiFi.begin(ssid_, password_);
    USB_DEBUG_LOGLN("[WiFi] restoring home STA for OTA");
  } else {
    USB_DEBUG_LOGLN("[WiFi] home SSID not configured");
  }
}

} // namespace soarm

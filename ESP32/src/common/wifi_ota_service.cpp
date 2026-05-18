#include "wifi_ota_service.h"

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
    WiFi.setHostname(hostname_);
    WiFi.begin(ssid_, password_);
    // Connection result is handled asynchronously in tick().

    ArduinoOTA.setHostname(hostname_);

    ArduinoOTA.onStart([this]() {
        otaInProgress_ = true;
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

void WifiOtaService::tick() {
    ArduinoOTA.handle();

    const bool connected = (WiFi.status() == WL_CONNECTED);

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
    return ipBuf_;
}

} // namespace soarm

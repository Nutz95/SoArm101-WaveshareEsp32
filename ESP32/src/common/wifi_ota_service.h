#pragma once

#include <cstdint>

namespace soarm {

struct WifiOtaCallbacks {
    void (*onWifiConnected)(const char *ipAddress) = nullptr;
    void (*onWifiDisconnected)()                   = nullptr;
    void (*onOtaBegin)()                           = nullptr;
    void (*onOtaEnd)()                             = nullptr;
    void (*onOtaError)(uint32_t errorCode)         = nullptr;
};

// Home-router STA + ArduinoOTA only (no direct AP/STA teleop link).
class WifiOtaService {
public:
    WifiOtaService(const char *ssid, const char *password, const char *hostname);

    void begin(WifiOtaCallbacks callbacks = {});
    void tick();

    void setStaConnectDesired(bool desired);
    bool isStaConnectDesired() const;

    bool isConnected()      const;
    bool isOtaInProgress()  const;
    const char *ipAddress() const;

private:
    const char *ssid_;
    const char *password_;
    const char *hostname_;
    WifiOtaCallbacks callbacks_;
    bool        otaInProgress_{false};
    bool        wasConnected_{false};
    bool        staConnectDesired_{true};
    char        ipBuf_[16]{};
};

} // namespace soarm

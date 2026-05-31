#pragma once

#include <cstdint>

namespace soarm {

// Callbacks to notify the app of WiFi/OTA lifecycle events.
struct WifiOtaCallbacks {
    void (*onWifiConnected)(const char *ipAddress) = nullptr;
    void (*onWifiDisconnected)()                   = nullptr;
    void (*onOtaBegin)()                           = nullptr;
    void (*onOtaEnd)()                             = nullptr;
    void (*onOtaError)(uint32_t errorCode)         = nullptr;
};

class WifiOtaService {
public:
    // ssid / password come from build flags (WIFI_SSID / WIFI_PASS macros).
    // hostname is used for mDNS and OTA target, e.g. "soarm-leader".
    WifiOtaService(const char *ssid, const char *password, const char *hostname);

    // Call once from app begin(). Returns immediately; connection is async.
    void begin(WifiOtaCallbacks callbacks = {});

    // Call every loop iteration (handles OTA packets and reconnection).
    void tick();

    // When false: disconnect from AP but keep Wi-Fi driver up for ESP-NOW. OTA forces reconnect.
    void setStaConnectDesired(bool desired);
    bool isStaConnectDesired() const;

    bool isConnected()      const;
    bool isOtaInProgress()  const;

    // Returns dotted-decimal IP string, or empty string if not connected.
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

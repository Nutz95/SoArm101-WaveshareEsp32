#pragma once

#include "../common/types/operation_mode.h"
#include "../common/teleop/teleop_transport_mode.h"
#include "oled_display_config.h"

#include <cstdint>

class Adafruit_SSD1306;

namespace soarm {

// Renders board state onto the leader's SSD1306 OLED display.
// Layout (128x64):
//   Line 0: hostname / role
//   Line 1: WiFi IP or "WiFi connecting..."
//   Line 2: State machine state
//   Line 3: Error code (hidden when no error)
class OledPresenter {
public:
    explicit OledPresenter(const OledDisplayConfig &config);
    ~OledPresenter();

    // Returns false if the display hardware is not found.
    bool begin();

    void showConnecting(const char *followerIpHint);
    void showDashboard(const char *leaderIp,
                       const char *followerIp,
                       OperationMode mode,
                       TeleopTransportMode transportMode,
                       const char *status,
                       uint32_t nowMs);
    void showOtaProgress(uint8_t progressPercent);
    void showError(uint32_t errorCode, const char *message);

    void showCalibrationAwaitEnter(const char *armLabel);
    void showCalibrationArmPrompt(const char *armLabel, uint32_t nowMs, uint32_t centerConfirmArmedAtMs);
    void showCalibrationCentering(const char *armLabel, const char *statusLine);
    void showCalibrationRangeTable(
        const char *row1,
        const char *row2,
        const char *row3,
        const char *footer);
    void showCalibrationResultBanner(const char *message);

    void showWifiDirectAwaitEnter(const char *leaderRouterIp, const char *followerRouterIp);
    void showWifiDirectWaitingFollower(const char *leaderApIp);
    void showWifiDirectAwaitStart(const char *leaderApIp, const char *followerApIp);

private:
    OledDisplayConfig config_;
    Adafruit_SSD1306 *display_;

    void applyTextStyle();
    void printScrollableLine(const char *line, uint8_t y, uint8_t visibleChars, uint32_t nowMs);
    void printLines(
        const char *line1,
        const char *line2,
        const char *line3,
        const char *line4);
};

} // namespace soarm

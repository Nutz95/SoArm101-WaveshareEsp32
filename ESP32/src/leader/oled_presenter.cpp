#include "oled_presenter.h"

#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include <cstring>

namespace soarm {

namespace {

const char *modeLabel(OperationMode mode, TeleopTransportMode transportMode) {
    switch (mode) {
        case OperationMode::Idle:
            return "IDLE";
        case OperationMode::CalibrationLeader:
            return "CAL LEADER";
        case OperationMode::CalibrationFollower:
            return "CAL FOLLOWER";
        case OperationMode::Teleoperation:
            if (transportMode == TeleopTransportMode::WifiUdp) {
                return "TELEOP WIFI";
            }
            return "TELEOP ESPNOW";
        case OperationMode::Passthrough:
            return "PASSTHROUGH";
        default:
            return "UNKNOWN";
    }
}

const char *shortIpTail(const char *ip) {
    static char tail[8];
    tail[0] = '\0';
    if (!ip || ip[0] == '\0') {
        return "?";
    }

    const char *lastDot = strrchr(ip, '.');
    if (!lastDot || *(lastDot + 1) == '\0') {
        strncpy(tail, ip, sizeof(tail) - 1);
        tail[sizeof(tail) - 1] = '\0';
        return tail;
    }

    // Keep only last octet for large-font page.
    strncpy(tail, lastDot + 1, sizeof(tail) - 1);
    tail[sizeof(tail) - 1] = '\0';
    return tail;
}

constexpr int kScreenWidth  = 128;
constexpr int kScreenHeight = 64;
constexpr int kOledReset    = -1; // Share Arduino reset pin.
constexpr uint32_t kScrollStepMs = 250U;
constexpr uint8_t kScrollHoldSteps = 14U;

} // namespace

OledPresenter::OledPresenter(const OledDisplayConfig &config)
        : config_(config), display_(nullptr) {}

OledPresenter::~OledPresenter() {
    if (display_ != nullptr) {
        delete display_;
        display_ = nullptr;
    }
}

bool OledPresenter::begin() {
    if (display_ != nullptr) {
        delete display_;
        display_ = nullptr;
    }

    display_ = new Adafruit_SSD1306(config_.screenWidth, config_.screenHeight, &Wire, kOledReset);
    if (display_ == nullptr) {
        return false;
    }

    if (!display_->begin(SSD1306_SWITCHCAPVCC, config_.i2cAddress)) {
        return false;
    }
    display_->setTextColor(SSD1306_WHITE);
    applyTextStyle();
    display_->clearDisplay();
    display_->display();
    return true;
}

void OledPresenter::showConnecting(const char *followerIpHint) {
    if (display_ == nullptr) {
        return;
    }

    char l2[24] = "F:unknown";
    if (followerIpHint && followerIpHint[0] != '\0') {
        snprintf(l2, sizeof(l2), "F:%s", followerIpHint);
    }

    printLines("L:connecting", l2, "M:IDLE", "S:WiFi connecting");
}

void OledPresenter::showDashboard(const char *leaderIp,
                                 const char *followerIp,
                                 OperationMode mode,
                                 TeleopTransportMode transportMode,
                                 const char *status,
                                 uint32_t nowMs) {
    if (display_ == nullptr) {
        return;
    }

    char l1[24] = "L:connecting";
    char l2[24] = "F:unknown";
    char l3[24] = "M:IDLE";
    char l4[24] = "S:OK";

    if (leaderIp && leaderIp[0] != '\0') {
        snprintf(l1, sizeof(l1), "L:%s", leaderIp);
    }
    if (followerIp && followerIp[0] != '\0') {
        snprintf(l2, sizeof(l2), "F:%s", followerIp);
    }
    snprintf(l3, sizeof(l3), "M:%s", modeLabel(mode, transportMode));
    if (status && status[0] != '\0') {
        snprintf(l4, sizeof(l4), "S:%s", status);
    }

    if (config_.alternatePages) {
        const uint32_t phaseMs = nowMs % 4000U;
        const bool compactPage = phaseMs < 3000U;
        if (!compactPage) {
            snprintf(l1, sizeof(l1), "L:%s", shortIpTail(leaderIp));
            snprintf(l2, sizeof(l2), "F:%s", shortIpTail(followerIp));
        }
    }

    printLines(l1, l2, l3, l4);
}

void OledPresenter::showOtaProgress(uint8_t progressPercent) {
    if (display_ == nullptr) {
        return;
    }

    display_->clearDisplay();
    display_->setFont();
    display_->setTextSize(1);
    display_->setCursor(0, 0);
    display_->println("OTA Update...");

    const int barWidth = (kScreenWidth * progressPercent) / 100;
    display_->fillRect(0, 20, barWidth, 10, SSD1306_WHITE);
    display_->drawRect(0, 20, kScreenWidth, 10, SSD1306_WHITE);

    display_->setCursor(0, 36);
    display_->print(progressPercent);
    display_->println("%");
    display_->display();
}

void OledPresenter::showOtaAwaitEnter(const char *routerIp) {
    char l2[24] = "L:?";
    if (routerIp != nullptr && routerIp[0] != '\0') {
        snprintf(l2, sizeof(l2), "L:%s", routerIp);
    }
    printLines("OTA UPDATE", l2, "router WiFi", "Enter? (A)");
}

void OledPresenter::showOtaActive(const char *routerIp) {
    char l2[24] = "L:?";
    if (routerIp != nullptr && routerIp[0] != '\0') {
        snprintf(l2, sizeof(l2), "L:%s", routerIp);
    }
    printLines("OTA ACTIVE", l2, "pio *-ota upload", "B: done");
}

void OledPresenter::showWifiDirectAwaitEnter(const char *leaderRouterIp, const char *followerRouterIp) {
    char l1[24] = "L:?";
    char l2[24] = "F:?";
    if (leaderRouterIp != nullptr && leaderRouterIp[0] != '\0') {
        snprintf(l1, sizeof(l1), "L:%s", leaderRouterIp);
    }
    if (followerRouterIp != nullptr && followerRouterIp[0] != '\0') {
        snprintf(l2, sizeof(l2), "F:%s", followerRouterIp);
    }
    printLines("TELEOP WIFI", l1, l2, "Enter? (A)");
}

void OledPresenter::showWifiDirectWaitingFollower(const char *leaderApIp) {
    char l2[24] = "L:AP ?";
    if (leaderApIp != nullptr && leaderApIp[0] != '\0') {
        snprintf(l2, sizeof(l2), "L:%s", leaderApIp);
    }
    printLines("TELEOP WIFI", l2, "waiting follower", "B:Cancel");
}

void OledPresenter::showWifiDirectAwaitStart(const char *leaderApIp, const char *followerApIp) {
    char l2[24] = "L:AP ?";
    char l3[24] = "F:STA ?";
    if (leaderApIp != nullptr && leaderApIp[0] != '\0') {
        snprintf(l2, sizeof(l2), "L:%s", leaderApIp);
    }
    if (followerApIp != nullptr && followerApIp[0] != '\0') {
        snprintf(l3, sizeof(l3), "F:%s", followerApIp);
    }
    printLines("TELEOP WIFI", l2, l3, "Start? (A)");
}

void OledPresenter::showCalibrationAwaitEnter(const char *armLabel) {
    char line2[22]{};
    snprintf(line2, sizeof(line2), "%s arm", armLabel != nullptr ? armLabel : "?");
    printLines("Calibration", line2, "Not started", "Enter? (A)");
}

void OledPresenter::showCalibrationArmPrompt(const char *armLabel,
                                             uint32_t nowMs,
                                             uint32_t centerConfirmArmedAtMs) {
    char line2[22]{};
    char line4[22]{};
    snprintf(line2, sizeof(line2), "%s arm", armLabel != nullptr ? armLabel : "?");

    if (centerConfirmArmedAtMs != 0U && nowMs < centerConfirmArmedAtMs) {
        const uint32_t waitSec = ((centerConfirmArmedAtMs - nowMs) + 999U) / 1000U;
        snprintf(line4, sizeof(line4), "Wait %lus", static_cast<unsigned long>(waitSec));
    } else {
        snprintf(line4, sizeof(line4), "A:Center B:Can");
    }

    printLines("Calibration", line2, "Place center", line4);
}

void OledPresenter::showCalibrationCentering(const char *armLabel, const char *statusLine) {
    char line2[22]{};
    snprintf(line2, sizeof(line2), "%s arm", armLabel != nullptr ? armLabel : "?");
    printLines(line2, statusLine != nullptr ? statusLine : "...", "", "");
}

void OledPresenter::showCalibrationRangeTable(
    const char *row1,
    const char *row2,
    const char *row3,
    const char *footer) {
    printLines(
        row1 != nullptr ? row1 : "",
        row2 != nullptr ? row2 : "",
        row3 != nullptr ? row3 : "",
        footer != nullptr ? footer : "A:Val B:Can");
}

void OledPresenter::showCalibrationResultBanner(const char *message) {
    printLines("", message != nullptr ? message : "OK", "", "");
}

void OledPresenter::showError(uint32_t errorCode, const char *message) {
    if (display_ == nullptr) {
        return;
    }

    display_->clearDisplay();
    display_->setFont();
    display_->setTextSize(1);
    display_->setCursor(0, 0);
    display_->println("** ERROR **");

    display_->setCursor(0, 14);
    display_->print("Code: 0x");
    display_->println(errorCode, HEX);

    display_->setCursor(0, 26);
    if (message) {
        display_->println(message);
    }
    display_->display();
}

void OledPresenter::applyTextStyle() {
    if (display_ == nullptr) {
        return;
    }

    switch (config_.textStyle) {
    case OledTextStyle::Small:
        display_->setFont();
        display_->setTextSize(1);
        break;
    case OledTextStyle::Medium:
        // Medium uses a proportional font from Adafruit GFX.
        // If the panel is too short (e.g. 128x32), fallback to small for 4-line layout.
        if (config_.screenHeight < 48U) {
            display_->setFont();
            display_->setTextSize(1);
        } else {
            display_->setFont(&FreeSans9pt7b);
            display_->setTextSize(1);
        }
        break;
    case OledTextStyle::Large:
        display_->setFont();
        display_->setTextSize(2);
        break;
    default:
        display_->setFont();
        display_->setTextSize(1);
        break;
    }
}

void OledPresenter::printScrollableLine(const char *line, uint8_t y, uint8_t visibleChars, uint32_t nowMs) {
    if (display_ == nullptr) {
        return;
    }

    const char *safeLine = (line != nullptr) ? line : "";
    const size_t lineLength = strlen(safeLine);
    display_->setCursor(0, y);
    if (lineLength <= visibleChars || visibleChars == 0U) {
        display_->println(safeLine);
        return;
    }

    const size_t maxOffset = lineLength - visibleChars;
    const uint32_t step = nowMs / kScrollStepMs;
    const uint32_t cycleLength = static_cast<uint32_t>(maxOffset + kScrollHoldSteps + 1U);
    size_t offset = 0U;
    if (cycleLength > 0U) {
        const uint32_t cycleIndex = step % cycleLength;
        offset = cycleIndex > kScrollHoldSteps ? static_cast<size_t>(cycleIndex - kScrollHoldSteps) : 0U;
        if (offset > maxOffset) {
            offset = maxOffset;
        }
    }

    char visibleBuffer[32]{};
    strncpy(visibleBuffer, safeLine + offset, visibleChars);
    visibleBuffer[visibleChars] = '\0';
    display_->println(visibleBuffer);
}

void OledPresenter::printLines(const char *line1,
                                                                const char *line2,
                                                                const char *line3,
                                                                const char *line4) {
    if (display_ == nullptr) {
        return;
    }

    display_->clearDisplay();
    applyTextStyle();

    uint8_t y0 = 0;
    uint8_t y1 = 8;
    uint8_t y2 = 16;
    uint8_t y3 = 24;

    if (config_.textStyle == OledTextStyle::Large) {
        y0 = 0;
        y1 = 16;
        y2 = 32;
        y3 = 48;
    } else {
        if (config_.textStyle == OledTextStyle::Medium && config_.screenHeight >= 48U) {
            y0 = 10;
            y1 = 24;
            y2 = 38;
            y3 = 52;
        }
    }

    uint8_t visibleChars = 21U;
    if (config_.textStyle == OledTextStyle::Large) {
        visibleChars = 10U;
    } else if (config_.textStyle == OledTextStyle::Medium && config_.screenHeight >= 48U) {
        visibleChars = 14U;
    }

    const uint32_t nowMs = millis();

    printScrollableLine(line1, y0, visibleChars, nowMs);
    printScrollableLine(line2, y1, visibleChars, nowMs);
    printScrollableLine(line3, y2, visibleChars, nowMs);
    printScrollableLine(line4, y3, visibleChars, nowMs);

    display_->display();
}

} // namespace soarm

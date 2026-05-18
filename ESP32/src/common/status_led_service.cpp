#include "status_led_service.h"

#include <Arduino.h>

namespace soarm {

StatusLedService::StatusLedService(uint8_t dataPin, uint16_t ledCount)
    : pixels_(ledCount, dataPin, NEO_GRB + NEO_KHZ800) {
}

void StatusLedService::begin() {
  pixels_.begin();
  pixels_.clear();
  pixels_.show();
}

void StatusLedService::render(uint16_t ledIndex, ArmRuntimeState state) {
  if (ledIndex >= pixels_.numPixels()) {
    return;
  }

  uint32_t color = makeColor(0, 0, 0);
  const bool blinkOn = isBlinkOn();

  switch (state) {
  case ArmRuntimeState::PairingOrUnpaired:
    color = blinkOn ? makeColor(0, 0, 40) : makeColor(0, 0, 0);
    break;
  case ArmRuntimeState::Paired:
    color = makeColor(0, 0, 60);
    break;
  case ArmRuntimeState::WaitingCalibration:
    color = blinkOn ? makeColor(60, 0, 0) : makeColor(0, 0, 0);
    break;
  case ArmRuntimeState::WaitingEspNow:
    color = blinkOn ? makeColor(0, 40, 0) : makeColor(0, 0, 0);
    break;
  case ArmRuntimeState::Ready:
    color = makeColor(0, 60, 0);
    break;
  default:
    color = makeColor(0, 0, 0);
    break;
  }

  pixels_.setPixelColor(ledIndex, color);
  pixels_.show();
}

uint32_t StatusLedService::makeColor(uint8_t red, uint8_t green, uint8_t blue) const {
  return pixels_.Color(red, green, blue);
}

bool StatusLedService::isBlinkOn() const {
  const uint32_t tick = millis() / 450;
  return (tick % 2U) == 0U;
}

} // namespace soarm

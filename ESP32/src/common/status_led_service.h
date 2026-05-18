#pragma once

#include "types/arm_runtime_state.h"
#include <Adafruit_NeoPixel.h>

namespace soarm {

class StatusLedService {
public:
  StatusLedService(uint8_t dataPin, uint16_t ledCount);
  void begin();
  void render(uint16_t ledIndex, ArmRuntimeState state);

private:
  uint32_t makeColor(uint8_t red, uint8_t green, uint8_t blue) const;
  bool isBlinkOn() const;

  Adafruit_NeoPixel pixels_;
};

} // namespace soarm

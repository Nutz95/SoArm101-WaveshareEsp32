#pragma once

#include <HardwareSerial.h>
#include <cstdint>

namespace soarm {

class LeaderServoPassthrough {
public:
  void enter(HardwareSerial &servoSerial);
  void exit();
  void tick(bool active);

private:
  HardwareSerial *servoSerial_{nullptr};
};

} // namespace soarm

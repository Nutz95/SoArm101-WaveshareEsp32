#include "leader_servo_passthrough.h"

#include "../Config/leader_runtime_config.h"

#include <Arduino.h>

namespace soarm {

void LeaderServoPassthrough::enter(HardwareSerial &servoSerial) {
  servoSerial_ = &servoSerial;
}

void LeaderServoPassthrough::exit() {
  servoSerial_ = nullptr;
}

void LeaderServoPassthrough::tick(bool active) {
  if (!active || servoSerial_ == nullptr) {
    return;
  }

  while (Serial.available() > 0) {
    servoSerial_->write(static_cast<uint8_t>(Serial.read()));
  }

  while (servoSerial_->available() > 0) {
    Serial.write(static_cast<uint8_t>(servoSerial_->read()));
  }
}

} // namespace soarm

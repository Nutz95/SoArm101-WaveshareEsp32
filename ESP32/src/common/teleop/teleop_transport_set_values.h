#pragma once

#include <cstdint>

namespace soarm {

// Values for dashboard command teleop_transport_set (LeaderCommandAction::TeleopTransportSet).
namespace teleop_transport_set {

constexpr uint32_t kEspNow = 0U;
constexpr uint32_t kWifiUdp = 1U;
constexpr uint32_t kCalibrationLeader = 2U;
constexpr uint32_t kCalibrationFollower = 3U;
constexpr uint32_t kPassthrough = 4U;
constexpr uint32_t kPcSerial = 5U;

} // namespace teleop_transport_set

} // namespace soarm

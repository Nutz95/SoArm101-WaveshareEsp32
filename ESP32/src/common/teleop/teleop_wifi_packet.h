#pragma once

#include "teleop_batch_packet.h"
#include "teleop_transport_mode.h"

#include <cstdint>

namespace soarm {

namespace teleop_wifi {

using BatchEntry = teleop_batch::BatchEntry;
using BatchPacket = teleop_batch::BatchPacket;
using AckPacket = teleop_batch::AckPacket;

constexpr uint16_t kMagic = teleop_batch::kMagic;
constexpr uint8_t kVersion = teleop_batch::kVersion;
constexpr uint8_t kTypeBatch = teleop_batch::kTypeBatch;
constexpr uint8_t kTypeAck = teleop_batch::kTypeAck;
constexpr uint16_t kFollowerListenPort = 29110U;

} // namespace teleop_wifi

} // namespace soarm

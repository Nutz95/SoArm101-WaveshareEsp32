#include "leader_teleop_pc_serial_bridge.h"



#include "../Config/common_runtime_config.h"

#include "../common/teleop/teleop_batch_packet.h"



namespace soarm {



void LeaderTeleopPcSerialBridge::attach(HardwareSerial &serial) {

  serial_ = &serial;

}



bool LeaderTeleopPcSerialBridge::sendBatch(

    const uint8_t *ids,

    const int16_t *positions,

    uint8_t count,

    uint8_t speedPercent,

    uint16_t requestId,

    uint8_t flags) {

  if (serial_ == nullptr || ids == nullptr || positions == nullptr || count == 0U) {

    return false;

  }



  const uint8_t clampedCount = (count > config::common::kTeleopBatchMaxServos)

                                  ? config::common::kTeleopBatchMaxServos

                                  : count;

  teleop_batch::BatchPacket packet{};

  packet.magic = teleop_batch::kMagic;

  packet.version = teleop_batch::kVersion;

  packet.flags = flags;

  packet.type = teleop_batch::kTypeBatch;

  packet.requestId = requestId;

  packet.count = clampedCount;

  packet.speedPercent = speedPercent;

  for (uint8_t i = 0U; i < clampedCount; ++i) {

    packet.entries[i].id = ids[i];

    packet.entries[i].position = positions[i];

  }



  const size_t written = serial_->write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));

  serial_->flush();

  return written == sizeof(packet);

}



} // namespace soarm


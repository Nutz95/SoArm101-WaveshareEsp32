#include "servo_bus_service.h"
#include "scoped_bus_lock.h"

#include "../../Config/common_runtime_config.h"

#include <SCServo.h>
#include <cstdio>
#include <cstring>

namespace soarm {

namespace {

uint8_t parseKnownIds(const char *idsText, uint8_t *ids, uint8_t maxCount) {
  if (idsText == nullptr || ids == nullptr || maxCount == 0U) {
    return 0U;
  }

  uint8_t count = 0U;
  const char *cursor = idsText;
  while (*cursor != '\0' && count < maxCount) {
    unsigned int id = 0U;
    if (sscanf(cursor, "%u", &id) == 1 && id <= 255U) {
      ids[count] = static_cast<uint8_t>(id & 0xFFU);
      ++count;
    }

    while (*cursor != '\0' && *cursor != ',') {
      ++cursor;
    }
    if (*cursor == ',') {
      ++cursor;
    }
  }

  return count;
}

} // namespace

uint8_t ServoBusService::refreshKnownTelemetryFast() {
  return refreshKnownTelemetrySync();
}

} // namespace soarm

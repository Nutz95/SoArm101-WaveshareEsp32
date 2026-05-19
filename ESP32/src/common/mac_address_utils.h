#pragma once

#include <cstdint>
#include <cstdio>

namespace soarm {

inline void formatMacAddress(const uint8_t mac[6], char out[18]) {
  snprintf(
      out,
      18,
      "%02X:%02X:%02X:%02X:%02X:%02X",
      mac[0],
      mac[1],
      mac[2],
      mac[3],
      mac[4],
      mac[5]);
}

} // namespace soarm
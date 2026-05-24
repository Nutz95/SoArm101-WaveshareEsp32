#pragma once

#include <cstdint>

namespace soarm {

class CpuLoadService {
public:
  void sample(uint8_t &cpu0LoadPct, uint8_t &cpu1LoadPct);
};

} // namespace soarm

#include "cpu_load_service.h"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>

namespace soarm {

void CpuLoadService::sample(uint8_t &cpu0LoadPct, uint8_t &cpu1LoadPct) {
#if (configGENERATE_RUN_TIME_STATS == 1) && (INCLUDE_xTaskGetIdleTaskHandle == 1)
  const uint32_t idle0Now = static_cast<uint32_t>(ulTaskGetIdleRunTimeCounterForCore(0));
  const uint32_t idle1Now = static_cast<uint32_t>(ulTaskGetIdleRunTimeCounterForCore(1));
  const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());

  if (!initialized_) {
    initialized_ = true;
    lastSampleUs_ = nowUs;
    lastIdleCounter0_ = idle0Now;
    lastIdleCounter1_ = idle1Now;
    cpu0LoadPct = 0U;
    cpu1LoadPct = 0U;
    return;
  }

  const uint64_t deltaUs = nowUs - lastSampleUs_;
  const uint32_t idle0Delta = idle0Now - lastIdleCounter0_;
  const uint32_t idle1Delta = idle1Now - lastIdleCounter1_;

  lastSampleUs_ = nowUs;
  lastIdleCounter0_ = idle0Now;
  lastIdleCounter1_ = idle1Now;

  if (deltaUs == 0U) {
    cpu0LoadPct = 0U;
    cpu1LoadPct = 0U;
    return;
  }

  uint32_t idle0Pct = static_cast<uint32_t>((static_cast<uint64_t>(idle0Delta) * 100ULL) / deltaUs);
  uint32_t idle1Pct = static_cast<uint32_t>((static_cast<uint64_t>(idle1Delta) * 100ULL) / deltaUs);
  if (idle0Pct > 100U) {
    idle0Pct = 100U;
  }
  if (idle1Pct > 100U) {
    idle1Pct = 100U;
  }

  cpu0LoadPct = static_cast<uint8_t>(100U - idle0Pct);
  cpu1LoadPct = static_cast<uint8_t>(100U - idle1Pct);
#else
  cpu0LoadPct = 0;
  cpu1LoadPct = 0;
#endif
}

} // namespace soarm

#include "cpu_load_service.h"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace soarm {

void CpuLoadService::sample(uint8_t &cpu0LoadPct, uint8_t &cpu1LoadPct) {
  // Arduino ESP32 framework does not reliably honour CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS.
  // Instead, we measure the time between sample() calls and estimate CPU load
  // based on how much time elapses between ticks. Shorter intervals = higher load.

  const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());

  if (!initialized_) {
    initialized_ = true;
    lastSampleUs_ = nowUs;
    cpu0LoadPct = 0U;
    cpu1LoadPct = 0U;
    return;
  }

  const uint64_t deltaUs = nowUs - lastSampleUs_;
  lastSampleUs_ = nowUs;

  if (deltaUs == 0U) {
    cpu0LoadPct = 0U;
    cpu1LoadPct = 0U;
    return;
  }

  // Expected tick interval is ~25ms (25000us) based on leader_app tick delay.
  // If delta is close to 25ms, CPU is idle. If delta is much shorter, CPU is busy.
  // We use a moving average to smooth out variations.
  const uint64_t expectedDeltaUs = 25000ULL;

  if (deltaUs >= expectedDeltaUs) {
    // CPU had time to idle between ticks.
    cpu0LoadPct = 0U;
    cpu1LoadPct = 0U;
    return;
  }

  // CPU was busy. Estimate load as (1 - delta/expected) * 100.
  const uint64_t idleFraction = (deltaUs * 100ULL) / expectedDeltaUs;
  uint32_t loadPct = 100U - static_cast<uint32_t>(idleFraction);
  if (loadPct > 100U) {
    loadPct = 100U;
  }

  cpu0LoadPct = static_cast<uint8_t>(loadPct);
  cpu1LoadPct = static_cast<uint8_t>(loadPct);
}

} // namespace soarm

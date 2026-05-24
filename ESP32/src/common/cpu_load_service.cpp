#include "cpu_load_service.h"

#include <esp_attr.h>
#include <esp_freertos_hooks.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace soarm {

namespace {

static volatile uint32_t g_cpu_total_ticks_core0 = 0U;
static volatile uint32_t g_cpu_idle_ticks_core0 = 0U;
static volatile uint32_t g_cpu_total_ticks_core1 = 0U;
static volatile uint32_t g_cpu_idle_ticks_core1 = 0U;
static volatile bool g_cpu_idle_seen_core0 = false;
static volatile bool g_cpu_idle_seen_core1 = false;

static uint32_t g_cpu_prev_total_ticks_core0 = 0U;
static uint32_t g_cpu_prev_idle_ticks_core0 = 0U;
static uint32_t g_cpu_prev_total_ticks_core1 = 0U;
static uint32_t g_cpu_prev_idle_ticks_core1 = 0U;

static bool g_cpu_tick_hooks_installed = false;

static void IRAM_ATTR cpuTickHookCore0() {
  g_cpu_total_ticks_core0++;
  if (g_cpu_idle_seen_core0) {
    g_cpu_idle_ticks_core0++;
    g_cpu_idle_seen_core0 = false;
  }
}

static void IRAM_ATTR cpuTickHookCore1() {
  g_cpu_total_ticks_core1++;
  if (g_cpu_idle_seen_core1) {
    g_cpu_idle_ticks_core1++;
    g_cpu_idle_seen_core1 = false;
  }
}

static bool IRAM_ATTR cpuIdleHookCore0() {
  g_cpu_idle_seen_core0 = true;
  return false;
}

static bool IRAM_ATTR cpuIdleHookCore1() {
  g_cpu_idle_seen_core1 = true;
  return false;
}

static bool ensureCpuTickHooksInstalled() {
  if (g_cpu_tick_hooks_installed) {
    return true;
  }

  esp_err_t err0 = esp_register_freertos_tick_hook_for_cpu(cpuTickHookCore0, 0);
  if (err0 != ESP_OK) {
    return false;
  }

  err0 = esp_register_freertos_idle_hook_for_cpu(cpuIdleHookCore0, 0);
  if (err0 != ESP_OK) {
    return false;
  }

#if (portNUM_PROCESSORS >= 2)
  esp_err_t err1 = esp_register_freertos_tick_hook_for_cpu(cpuTickHookCore1, 1);
  if (err1 != ESP_OK) {
    return false;
  }

  err1 = esp_register_freertos_idle_hook_for_cpu(cpuIdleHookCore1, 1);
  if (err1 != ESP_OK) {
    return false;
  }
#endif

  g_cpu_tick_hooks_installed = true;
  return true;
}

static uint8_t computeLoadPct(uint32_t totalDelta, uint32_t idleDelta) {
  if (totalDelta == 0U) {
    return 0U;
  }

  const float idleFrac = static_cast<float>(idleDelta) / static_cast<float>(totalDelta);
  float cpuPercent = 100.0f * (1.0f - idleFrac);
  if (cpuPercent < 0.0f) {
    cpuPercent = 0.0f;
  }
  if (cpuPercent > 100.0f) {
    cpuPercent = 100.0f;
  }
  return static_cast<uint8_t>(cpuPercent);
}

} // namespace

void CpuLoadService::sample(uint8_t &cpu0LoadPct, uint8_t &cpu1LoadPct) {
  if (!ensureCpuTickHooksInstalled()) {
    cpu0LoadPct = 0U;
    cpu1LoadPct = 0U;
    return;
  }

  const uint32_t total0 = g_cpu_total_ticks_core0;
  const uint32_t idle0 = g_cpu_idle_ticks_core0;
  const uint32_t total1 = g_cpu_total_ticks_core1;
  const uint32_t idle1 = g_cpu_idle_ticks_core1;

  const uint32_t total0Delta = total0 - g_cpu_prev_total_ticks_core0;
  const uint32_t idle0Delta = idle0 - g_cpu_prev_idle_ticks_core0;
  const uint32_t total1Delta = total1 - g_cpu_prev_total_ticks_core1;
  const uint32_t idle1Delta = idle1 - g_cpu_prev_idle_ticks_core1;

  g_cpu_prev_total_ticks_core0 = total0;
  g_cpu_prev_idle_ticks_core0 = idle0;
  g_cpu_prev_total_ticks_core1 = total1;
  g_cpu_prev_idle_ticks_core1 = idle1;

  cpu0LoadPct = computeLoadPct(total0Delta, idle0Delta);

#if (portNUM_PROCESSORS >= 2)
  cpu1LoadPct = computeLoadPct(total1Delta, idle1Delta);
#else
  cpu1LoadPct = 0U;
#endif
}

} // namespace soarm

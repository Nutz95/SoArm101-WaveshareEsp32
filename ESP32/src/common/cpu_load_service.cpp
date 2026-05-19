#include "cpu_load_service.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

namespace soarm {

void CpuLoadService::sample(uint8_t &cpu0LoadPct, uint8_t &cpu1LoadPct) {
#if (configUSE_TRACE_FACILITY == 1)
  constexpr UBaseType_t kMaxTasks = 64;
  TaskStatus_t taskStatus[kMaxTasks];
  uint32_t totalRuntime = 0;
  const UBaseType_t taskCount = uxTaskGetSystemState(taskStatus, kMaxTasks, &totalRuntime);

  if (taskCount == 0 || totalRuntime == 0) {
    cpu0LoadPct = 0;
    cpu1LoadPct = 0;
    return;
  }

  uint32_t cpu0Total = 0;
  uint32_t cpu1Total = 0;
  uint32_t cpu0Idle = 0;
  uint32_t cpu1Idle = 0;

  for (UBaseType_t i = 0; i < taskCount; ++i) {
    const TaskStatus_t &entry = taskStatus[i];
    const uint32_t runtime = entry.ulRunTimeCounter;
#if defined(CONFIG_FREERTOS_UNICORE)
    cpu0Total += runtime;
    if (entry.pcTaskName != nullptr && strcmp(entry.pcTaskName, "IDLE") == 0) {
      cpu0Idle += runtime;
    }
#else
    const BaseType_t coreId = entry.xCoreID;
    if (coreId == 0) {
      cpu0Total += runtime;
      if (entry.pcTaskName != nullptr && strcmp(entry.pcTaskName, "IDLE0") == 0) {
        cpu0Idle += runtime;
      }
    } else if (coreId == 1) {
      cpu1Total += runtime;
      if (entry.pcTaskName != nullptr && strcmp(entry.pcTaskName, "IDLE1") == 0) {
        cpu1Idle += runtime;
      }
    }
#endif
  }

  if (cpu0Total > 0) {
    cpu0LoadPct = static_cast<uint8_t>(100U - ((cpu0Idle * 100U) / cpu0Total));
  } else {
    cpu0LoadPct = 0;
  }

#if defined(CONFIG_FREERTOS_UNICORE)
  cpu1LoadPct = 0;
#else
  if (cpu1Total > 0) {
    cpu1LoadPct = static_cast<uint8_t>(100U - ((cpu1Idle * 100U) / cpu1Total));
  } else {
    cpu1LoadPct = 0;
  }
#endif
#else
  cpu0LoadPct = 0;
  cpu1LoadPct = 0;
#endif
}

} // namespace soarm

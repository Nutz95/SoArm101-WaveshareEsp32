#include "usb_debug_log_gate.h"

#include <Arduino.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace soarm {

namespace {

std::atomic<bool> g_usbDashboardStreamActive{false};

} // namespace

void setUsbDashboardStreamActive(bool active) {
  g_usbDashboardStreamActive.store(active);
}

bool isUsbDashboardStreamActive() {
  return g_usbDashboardStreamActive.load();
}

void usbDebugLogPrintf(const char *format, ...) {
#if defined(APP_ROLE_LEADER) && APP_ROLE_LEADER
  if (isUsbDashboardStreamActive()) {
    return;
  }
#endif
  va_list args;
  va_start(args, format);
  char buffer[192]{};
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  Serial.print(buffer);
}

void usbDebugLogPrintln(const char *line) {
#if defined(APP_ROLE_LEADER) && APP_ROLE_LEADER
  if (isUsbDashboardStreamActive()) {
    return;
  }
#endif
  if (line != nullptr) {
    Serial.println(line);
  }
}

} // namespace soarm

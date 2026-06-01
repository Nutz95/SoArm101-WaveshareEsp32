#pragma once

#include <cstdarg>
#include <cstdint>

namespace soarm {

// When true, leader USB CDC carries binary dashboard frames only (no text logs on Serial).
void setUsbDashboardStreamActive(bool active);
bool isUsbDashboardStreamActive();

void usbDebugLogPrintf(const char *format, ...);
void usbDebugLogPrintln(const char *line);

} // namespace soarm

#if defined(APP_ROLE_LEADER) && APP_ROLE_LEADER
#define USB_DEBUG_LOGF(...) ::soarm::usbDebugLogPrintf(__VA_ARGS__)
#define USB_DEBUG_LOGLN(line) ::soarm::usbDebugLogPrintln(line)
#else
#define USB_DEBUG_LOGF(...) Serial.printf(__VA_ARGS__)
#define USB_DEBUG_LOGLN(line) Serial.println(line)
#endif

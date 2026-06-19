#pragma once

#include "leader_xbox_controller_service.h"

#include <NimBLEDevice.h>

namespace soarm {

class LeaderXboxControllerService;

extern LeaderXboxControllerService *g_xboxService;
extern NimBLEClient *g_xboxClient;
extern NimBLEAdvertisedDevice *g_xboxTargetDevice;
extern XboxClientCallbacks g_xboxClientCallbacks;

void xboxInputReportCallback(NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify);

} // namespace soarm

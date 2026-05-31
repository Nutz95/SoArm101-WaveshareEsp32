#include "leader_radio_coexistence.h"

#include <esp_coexist.h>

namespace soarm {

void applyLeaderRadioCoexistencePreference() {
  // Do not call esp_bt_controller_mem_release here: NimBLEDevice::init() already does that once.
  esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
}

} // namespace soarm

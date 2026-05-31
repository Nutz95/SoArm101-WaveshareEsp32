#pragma once

namespace soarm {

// Call after NimBLEDevice::init(); do not release BT memory here because NimBLE already does.
void applyLeaderRadioCoexistencePreference();

} // namespace soarm

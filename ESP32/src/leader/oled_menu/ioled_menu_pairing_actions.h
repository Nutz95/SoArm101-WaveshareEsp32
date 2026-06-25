#pragma once

namespace soarm {

// Callback from pairing confirm screen to reset ESP-NOW pairing on the leader.
class IOledMenuPairingActions {
public:
  virtual ~IOledMenuPairingActions() = default;

  virtual bool resetPairingFromMenu() = 0;
};

} // namespace soarm

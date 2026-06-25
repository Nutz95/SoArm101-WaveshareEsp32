#pragma once

namespace soarm {

class FollowerApp;

class FollowerTeleopLoadSamplerTask {
public:
  static void runLoop(FollowerApp &app);
};

} // namespace soarm

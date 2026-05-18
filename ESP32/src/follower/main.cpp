#include "follower_app.h"

soarm::FollowerApp g_followerApp;

void setup() {
  g_followerApp.begin();
}

void loop() {
  g_followerApp.tick();
}

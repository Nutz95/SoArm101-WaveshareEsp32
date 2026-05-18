#include "leader_app.h"

soarm::LeaderApp g_leaderApp;

void setup() {
  g_leaderApp.begin();
}

void loop() {
  g_leaderApp.tick();
}

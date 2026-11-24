#pragma once

#include "apps/app.h"

// Temporary: two Stickies find each other over ESP-NOW and pass a counter back and
// forth, to prove the radio before a real game uses it. Removed once Yahtzee plays
// between two devices. See README.md.
App& linkTestApp();

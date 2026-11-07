#pragma once

#include <cstdint>

// About: the logo, the version and build, how many games there are, and where the code is.
namespace aboutScreen {

// True if the tap was BACK, which closes it.
bool onTap(int x, int y);

// `gameCount` comes from the shell, since settings can't see the games.
void draw(uint8_t* frame, int gameCount);

}  // namespace aboutScreen

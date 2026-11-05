#pragma once

#include <cstdint>

// The home page: one row per game, with Settings fixed at the bottom.
namespace homeScreen {

constexpr int TAP_NOTHING = -1;
constexpr int TAP_SETTINGS = -2;

void draw(uint8_t* frame);

// Returns the index of the tapped game, or one of the values above.
int onTap(int x, int y);

}  // namespace homeScreen

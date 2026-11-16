#pragma once

#include <cstdint>

// The APPS page, opened from the icon beside the cog on the home page: one row per
// app, under a band with BACK. One page holds every app for now, so it has no paging
// and Up and Down do nothing here; that changes past ROWS apps (apps_screen.cpp).
namespace appsScreen {

constexpr int TAP_NOTHING = -1;
constexpr int TAP_BACK = -2;

void draw(uint8_t* frame);

// Returns the index of the tapped app, or one of the values above.
int onTap(int x, int y);

}  // namespace appsScreen

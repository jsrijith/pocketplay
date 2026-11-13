#pragma once

#include <cstdint>

// The home page, in CrossPlay's look: a page of game rows, numbered page marks when
// there's more than one page, and Settings in a bar along the bottom.
namespace homeScreen {

constexpr int TAP_NOTHING = -1;
constexpr int TAP_SETTINGS = -2;
constexpr int TAP_PAGE = -3;  // the page changed, so redraw

// Reopens the page that was showing before sleep. Call after storage::begin().
void begin();

void draw(uint8_t* frame);

// Call every loop while the page is open. Returns true if the battery on it needs a
// redraw: charging started or stopped, or the percent moved while charging.
bool update();

// Returns the index of the tapped game, or one of the values above.
int onTap(int x, int y);

// Step one page. Return true if the page changed.
bool onUpPressed();
bool onDownPressed();

// True if the page couldn't be saved, which only costs where the page reopens.
bool saveFailed();

}  // namespace homeScreen

#pragma once

#include <cstdint>

// The time zone list. Picking a zone saves it and closes the list.
namespace timezoneScreen {

// Opens on the page holding the current zone.
void open();
bool isOpen();

// These return true if the display needs redrawing.
bool onTap(int x, int y);
bool onUpPressed();
bool onDownPressed();

void draw(uint8_t* frame);

}  // namespace timezoneScreen

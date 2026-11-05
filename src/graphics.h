#pragma once

#include <EInkDisplay.h>

#include <cstdint>

// The panel is natively landscape. The firmware draws in portrait, held with the
// USB edge on the right. setPixel() converts between the two.
constexpr int SCREEN_WIDTH = EInkDisplay::DISPLAY_HEIGHT;
constexpr int SCREEN_HEIGHT = EInkDisplay::DISPLAY_WIDTH;

// Anything outside the screen is skipped, so callers can draw across an edge.
void setPixel(uint8_t* frame, int x, int y, bool black);
void fillRect(uint8_t* frame, int left, int top, int width, int height, bool black);
void drawRectOutline(uint8_t* frame, int left, int top, int width, int height, int thickness);

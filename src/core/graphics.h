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

// A circle outline. radius is its outer edge, and thickness grows inward.
void drawRing(uint8_t* frame, int centerX, int centerY, int radius, int thickness, bool black);

// A rectangle with rounded corners. radius is the corners' outer edge, and an
// outline's thickness grows inward, like drawRing().
void fillRoundRect(uint8_t* frame, int left, int top, int width, int height, int radius, bool black);
void drawRoundRectOutline(uint8_t* frame, int left, int top, int width, int height, int radius, int thickness, bool black = true);

// A 1-bit icon in CrossPlay's format: rows padded to whole bytes, most significant
// bit first, 0 = ink. Only the ink is drawn, so an icon can sit on black or white.
void drawIcon(uint8_t* frame, const uint8_t* bits, int width, int height, int left, int top, bool black = true);

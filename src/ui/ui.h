#pragma once

#include <cstdint>

// The parts every screen shares, so they all line up and behave the same.
namespace ui {

constexpr int MARGIN = 12;
constexpr int GAP = 12;

constexpr int HEADER_TOP = 8;
constexpr int HEADER_BUTTON_WIDTH = 150;
constexpr int HEADER_BUTTON_HEIGHT = 64;

// The warning line, under the header. Content starts below it, so a warning never
// has to move anything.
constexpr int WARNING_TOP = 76;
constexpr int CONTENT_TOP = 108;

bool isInBox(int x, int y, int left, int top, int width, int height);

void drawButton(uint8_t* frame, int left, int top, int width, int height, const char* label);

// BACK on the left, and the title centred in the space beside it. Set hasRightButton
// if the screen draws a button of its own in the top right corner.
void drawHeader(uint8_t* frame, const char* title, bool hasRightButton = false);
bool isOnBack(int x, int y);

void drawWarning(uint8_t* frame, const char* text);

}  // namespace ui

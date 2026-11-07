#pragma once

#include <cstdint>

// PocketPlay's logo: a die peeking out of a stitched pocket, drawn in outline so it costs
// the panel little ink. Portrait only.
namespace ui {

constexpr int LOGO_WIDTH = 270;
constexpr int LOGO_HEIGHT = 400;  // from the die's top corner to the pocket's point

// The logo with its top centre at (centerX, top).
void drawLogo(uint8_t* frame, int centerX, int top);

// The boot and sleep screen: the logo, POCKETPLAY under it, `line` under that, and the
// version along the bottom.
void drawSplash(uint8_t* frame, const char* line);

// "VERSION 0.1.0  BUILD 55 (F851F9C)".
void formatVersion(char* out, int size);

}  // namespace ui

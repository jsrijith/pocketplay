#pragma once

#include <cstdint>

#include "core/text.h"

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

// CrossPlay's Toybox look, for screens moved over to it. Values from its
// ToyboxMetrics.h.
constexpr int HAIRLINE = 1;
constexpr int RULE = 3;
constexpr int FRAME = 4;
constexpr int BOARD_FRAME = 9;
constexpr int ROW_HEIGHT = 62;
constexpr int ROW_GAP = 4;
constexpr int PILL_HEIGHT = 52;
constexpr int PILL_RADIUS = 20;
constexpr int ICON_SIZE = 32;  // a game's home-page icon: 4 bytes a row

// The solid black header, then a white gap and a rule. It ends above WARNING_TOP,
// so the warning line and CONTENT_TOP are where they always are. The title is the
// largest cut that fits, never shortened. rightLabel, such as "2/3", can be null.
// With hasBack, BACK sits where isOnBack() looks.
constexpr int BAND_HEIGHT = 66;
void drawBand(uint8_t* frame, const char* title, const char* rightLabel, bool hasBack, Font labelFont = Font::Size30);

// A rounded button. A filled one is black with white type.
void drawPill(uint8_t* frame, int left, int top, int width, const char* label, bool filled, int height = PILL_HEIGHT);

// A button that can't act: dimmed, never removed, so the layout doesn't jump and it
// still says what it would do. A light grey fill inside a thin outline, with the
// label still black, because type can't be drawn grey.
void drawDimmedPill(uint8_t* frame, int left, int top, int width, const char* label, int height = PILL_HEIGHT);

// A list row: the label on the left and a 32 px icon (or none) on the right.
void drawRow(uint8_t* frame, int top, const char* label, const uint8_t* icon);

// Left-aligned text with its capitals centred in a box of the given height.
void drawTextLeft(uint8_t* frame, Font font, int left, int top, int height, const char* text, bool black = true);

}  // namespace ui

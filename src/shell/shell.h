#pragma once

#include <cstdint>

// Which screen is open, where taps go, and which game to reopen after waking. It
// makes no hardware calls of its own: main.cpp reads the input and runs the panel.
namespace shell {

enum class Redraw { None, Fast, Full };

// Reads the saves and reopens the game that was open. Call after storage::begin().
void begin(bool touchWorks);

// Call once per loop.
Redraw update();

Redraw onTap(int x, int y);
Redraw onUpPressed();
Redraw onDownPressed();

// Sets the orientation for what's on screen, then draws it.
void draw(uint8_t* frame);

// True while a landscape game is open, so taps are read in its frame.
bool landscape();

// True while a game is doing work that a sleep would throw away.
bool busy();

// Screen updates between clean-up refreshes. 0 means never.
int updatesPerCleanup();

uint32_t idleSleepMs();

// Turns the radio off and retries any save that failed. Call before cutting power.
void beforeSleep();

}  // namespace shell

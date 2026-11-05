#pragma once

#include <cstdint>

// The settings page, and the saved settings behind it.
namespace settingsScreen {

enum class Tap { Nothing, Changed, Back, OpenWifi };

// Reads the saved settings. Call after storage::begin().
void begin();

uint8_t idleSleepMinutes();

Tap onTap(int x, int y);
void draw(uint8_t* frame);

bool saveFailed();
void retrySave();

}  // namespace settingsScreen

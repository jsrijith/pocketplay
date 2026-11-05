#pragma once

#include <cstdint>

// The settings page, and the saved settings behind it.
namespace settingsScreen {

enum class Tap { Nothing, Changed, Back, OpenWifi, OpenTimeZone };

// Reads the saved settings. Call after storage::begin().
void begin();

uint8_t idleSleepMinutes();

// Screen updates between clean-up refreshes. 0 means never.
int updatesPerCleanup();

int timeZone();
void setTimeZone(int index);  // saves it and applies it to the clock

Tap onTap(int x, int y);
void draw(uint8_t* frame);

bool saveFailed();
void retrySave();

}  // namespace settingsScreen

#pragma once

#include <cstddef>
#include <cstdint>

// What survives sleep, and how it turns into bytes for a save file. Values are
// written one at a time rather than copying the struct, so the file layout is a
// decision here instead of whatever the compiler's padding happens to be.

constexpr uint16_t COUNT_LIMIT = 10000;  // wraps to 0, so a count never exceeds 4 digits

constexpr int BUTTON_COUNT = 3;

enum class Swipe : uint8_t { None, Left, Right, Up, Down };

struct Settings {
  uint8_t idleSleepMinutes = 10;  // 1, 5 or 10
};

struct GameState {
  uint16_t buttonCounts[BUTTON_COUNT] = {0, 0, 0};
  uint16_t tapCount = 0;
  uint16_t swipeCount = 0;
  bool hasTapped = false;
  int16_t tapX = 0;
  int16_t tapY = 0;
  Swipe swipe = Swipe::None;
};

constexpr uint8_t SETTINGS_VERSION = 1;
constexpr size_t SETTINGS_BYTES = 1;

constexpr uint8_t GAME_VERSION = 1;
constexpr size_t GAME_BYTES = 16;

size_t packSettings(const Settings& settings, uint8_t* out);
size_t packGame(const GameState& game, uint8_t* out);

// Return false and leave the result alone if the bytes aren't something this
// firmware can trust: too short, a newer version, or a value out of range.
bool unpackSettings(uint8_t version, const uint8_t* in, size_t size, Settings& settings);
bool unpackGame(uint8_t version, const uint8_t* in, size_t size, GameState& game);

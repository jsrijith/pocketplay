#include "state.h"

#include "graphics.h"

namespace {

void put16(uint8_t*& out, uint16_t value) {
  *out++ = static_cast<uint8_t>(value & 0xFF);
  *out++ = static_cast<uint8_t>(value >> 8);
}

uint16_t take16(const uint8_t*& in) {
  const uint16_t low = *in++;
  const uint16_t high = *in++;
  return static_cast<uint16_t>(low | (high << 8));
}

bool validIdleMinutes(uint8_t minutes) { return minutes == 1 || minutes == 5 || minutes == 10; }

}  // namespace

size_t packSettings(const Settings& settings, uint8_t* out) {
  out[0] = settings.idleSleepMinutes;
  return SETTINGS_BYTES;
}

bool unpackSettings(uint8_t version, const uint8_t* in, size_t size, Settings& settings) {
  if (version != SETTINGS_VERSION || size != SETTINGS_BYTES) return false;
  if (!validIdleMinutes(in[0])) return false;

  settings.idleSleepMinutes = in[0];
  return true;
}

size_t packGame(const GameState& game, uint8_t* out) {
  uint8_t* cursor = out;
  for (const uint16_t count : game.buttonCounts) {
    put16(cursor, count);
  }
  put16(cursor, game.tapCount);
  put16(cursor, game.swipeCount);
  put16(cursor, static_cast<uint16_t>(game.tapX));
  put16(cursor, static_cast<uint16_t>(game.tapY));
  *cursor++ = game.hasTapped ? 1 : 0;
  *cursor++ = static_cast<uint8_t>(game.swipe);
  return static_cast<size_t>(cursor - out);
}

bool unpackGame(uint8_t version, const uint8_t* in, size_t size, GameState& game) {
  if (version != GAME_VERSION || size != GAME_BYTES) return false;

  GameState loaded;
  const uint8_t* cursor = in;
  for (uint16_t& count : loaded.buttonCounts) {
    count = take16(cursor);
    if (count >= COUNT_LIMIT) return false;
  }
  loaded.tapCount = take16(cursor);
  loaded.swipeCount = take16(cursor);
  if (loaded.tapCount >= COUNT_LIMIT || loaded.swipeCount >= COUNT_LIMIT) return false;

  loaded.tapX = static_cast<int16_t>(take16(cursor));
  loaded.tapY = static_cast<int16_t>(take16(cursor));
  if (loaded.tapX < 0 || loaded.tapX >= SCREEN_WIDTH || loaded.tapY < 0 || loaded.tapY >= SCREEN_HEIGHT) return false;

  const uint8_t hasTapped = *cursor++;
  const uint8_t swipe = *cursor++;
  if (hasTapped > 1 || swipe > static_cast<uint8_t>(Swipe::Down)) return false;
  loaded.hasTapped = hasTapped == 1;
  loaded.swipe = static_cast<Swipe>(swipe);

  game = loaded;
  return true;
}

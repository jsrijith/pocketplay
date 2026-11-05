#pragma once

#include <cstddef>
#include <cstdint>

#include "wordle_rules.h"

// The game on screen, as bytes. It holds the day and the row being typed. Submitted
// guesses are in the games file, so they aren't repeated here.
namespace wordle {

constexpr uint8_t SAVE_VERSION = 1;
constexpr size_t SAVE_BYTES = 3 + kLetters;  // 16-bit day, typed count, letters

struct OpenGame {
  int day = -1;
  int typed = 0;
  char letters[kLetters] = {};
};

size_t packOpenGame(const OpenGame& game, uint8_t* out);

// Returns false and leaves the result alone if the bytes can't be trusted.
bool unpackOpenGame(uint8_t version, const uint8_t* in, size_t size, OpenGame& game);

}  // namespace wordle

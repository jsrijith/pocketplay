#pragma once

#include <cstddef>
#include <cstdint>

#include "diceroller_rules.h"

// The last roll as bytes, so a wake shows the same dice: one byte a die, 1 to 6.
namespace diceroller {

constexpr uint8_t SAVE_VERSION = 1;
constexpr size_t SAVE_BYTES = kDice;

size_t pack(const Roll& roll, uint8_t* out);

// Returns false and leaves the roll alone unless every die is 1 to 6.
bool unpack(uint8_t version, const uint8_t* in, size_t size, Roll& roll);

}  // namespace diceroller

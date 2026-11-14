#pragma once

#include <cstddef>
#include <cstdint>

#include "crowncrest_rules.h"

// The last roll as bytes, one face a die, so sleep or a flat battery brings back the same dice.
namespace crowncrest {

constexpr uint8_t SAVE_VERSION = 1;
constexpr size_t SAVE_BYTES = kDice;

size_t pack(const Roll& roll, uint8_t* out);

// Returns false and leaves the roll alone unless every die shows a face from 1 to 6.
bool unpack(uint8_t version, const uint8_t* in, size_t size, Roll& roll);

}  // namespace crowncrest

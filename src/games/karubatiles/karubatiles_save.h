#pragma once

#include <cstddef>
#include <cstdint>

#include "karubatiles_rules.h"

// The round in play as bytes, so sleep or a flat battery keeps every draw: the count, then each
// draw in order, one byte each.
namespace karubatiles {

constexpr uint8_t SAVE_VERSION = 1;
constexpr size_t SAVE_HEADER = 1;
constexpr size_t MAX_SAVE_BYTES = SAVE_HEADER + kTiles;  // 37

size_t pack(const State& state, uint8_t* out);

// Returns false and leaves the state alone unless the size matches the count and every draw is
// a tile from 1 to 36 that hasn't come before.
bool unpack(uint8_t version, const uint8_t* in, size_t size, State& state);

}  // namespace karubatiles

#pragma once

#include <cstddef>
#include <cstdint>

#include "calculator_rules.h"

// The calculator's state as bytes, packed one value at a time rather than as a copied
// struct, so the layout is fixed whatever the compiler does. Raise SAVE_VERSION when
// the layout changes.
namespace calculator {

constexpr uint8_t SAVE_VERSION = 1;

// The entry's length and characters, two 8-byte numbers, two operators and the flags.
constexpr size_t SAVE_BYTES = 1 + MAX_ENTRY + 8 + 8 + 1 + 1 + 1;

size_t pack(const State& state, uint8_t* out);

// Returns false, leaving state alone, for a wrong version or any value out of range.
bool unpack(uint8_t version, const uint8_t* data, size_t size, State& state);

}  // namespace calculator

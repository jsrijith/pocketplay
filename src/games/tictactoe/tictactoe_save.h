#pragma once

#include <cstddef>
#include <cstdint>

#include "tictactoe_rules.h"

// A game in progress as bytes, with no screen or card, so it can be tested off the
// device. Only a running game is ever saved: a finished one erases its save.
namespace tictactoe {

// Who's playing. The order is saved, so only add to the end.
enum class Mode : uint8_t { TwoPlayers, Easy, Hard };
constexpr uint8_t kModeCount = 3;

struct Saved {
  Game game;
  Mode mode;
  // The computer's mark, kX or kO, and kEmpty with two players.
  uint8_t computer;
};

// Version 1 held the board and the turn, from before the computer could play. It
// still loads, as a game between two players.
constexpr uint8_t SAVE_VERSION = 2;
constexpr size_t SAVE_BYTES_V1 = kCells + 1;
constexpr size_t SAVE_BYTES = kCells + 3;

size_t packGame(const Saved& saved, uint8_t* out);

// Returns false and leaves `saved` alone unless the bytes are a position that real
// play can reach and that isn't already finished, with a mode that fits it.
bool unpackGame(uint8_t version, const uint8_t* in, size_t size, Saved& saved);

}  // namespace tictactoe

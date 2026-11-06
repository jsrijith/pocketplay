#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// The record, CrossPlay's: one byte per finished game, oldest first, in the blob
// named "solitaire-results". A deal that was won is RESULT_WON, and one left for a
// new deal after a move is RESULT_LOST. No screen or card code, so
// tests/solitaire/ can check it.
namespace solitaire {

constexpr uint8_t RESULT_LOST = 1;
constexpr uint8_t RESULT_WON = 2;
constexpr size_t MAX_RESULTS = 2048;
constexpr int RECENT_GAMES = 16;

// A full record drops its oldest game rather than stop counting.
void appendResult(std::vector<uint8_t>& results, bool won);

// False for a record that's too long or holds a byte other than the two results.
bool resultsValid(const uint8_t* data, size_t size);

struct Record {
  int played = 0;
  int wins = 0;
  int streak = 0;  // wins in a row, back from the newest game
  // The last sixteen games, oldest first: 0 not played yet, else a result.
  uint8_t recent[RECENT_GAMES] = {};
};

Record summarise(const std::vector<uint8_t>& results);

}  // namespace solitaire

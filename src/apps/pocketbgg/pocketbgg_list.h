#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// A collection of games, and how it splits into pages. No screen or card, so it can be tested
// off the device: the screen measures each row and hands the heights in.
namespace pocketbgg {

constexpr int16_t kNoYear = INT16_MIN;  // BGG sends no year for some games

struct Game {
  uint32_t id;       // BGG's objectid for the game
  const char* name;  // ASCII: PocketPlay's font draws nothing else
  int16_t year;      // negative is BC, as BGG has Nine Men's Morris at -1400
  uint16_t plays;
};

// The line under a game's name, such as "2020  -  33 PLAYS", "1400 BC  -  3 PLAYS", or
// "1 PLAY" when there's no year.
void describe(const Game& game, char* out, size_t size);

// Splits rows of the given heights into pages, each holding as many whole rows as fit in `room`
// with `gap` between them. Returns the first row of each page. A row taller than the room gets
// a page of its own, so every row is on some page. No rows gives one empty page.
std::vector<int> paginate(const std::vector<int>& heights, int room, int gap);

// Which page a row is on, given paginate()'s answer.
int pageOf(const std::vector<int>& firstRows, int row);

}  // namespace pocketbgg

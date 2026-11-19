#include "pocketbgg_list.h"

#include <cstdio>

namespace pocketbgg {

void describe(const Game& game, char* out, size_t size) {
  const char* noun = game.plays == 1 ? "PLAY" : "PLAYS";
  if (game.year == kNoYear) {
    std::snprintf(out, size, "%u %s", static_cast<unsigned>(game.plays), noun);
  } else if (game.year < 0) {
    std::snprintf(out, size, "%d BC  -  %u %s", -game.year, static_cast<unsigned>(game.plays), noun);
  } else {
    std::snprintf(out, size, "%d  -  %u %s", game.year, static_cast<unsigned>(game.plays), noun);
  }
}

std::vector<int> paginate(const std::vector<int>& heights, int room, int gap) {
  std::vector<int> firstRows{0};
  int used = 0;
  for (int row = 0; row < static_cast<int>(heights.size()); row++) {
    const int needed = (used == 0 ? 0 : gap) + heights[row];
    if (used != 0 && used + needed > room) {
      firstRows.push_back(row);
      used = heights[row];
    } else {
      used += needed;
    }
  }
  return firstRows;
}

int pageOf(const std::vector<int>& firstRows, int row) {
  int page = 0;
  for (int p = 0; p < static_cast<int>(firstRows.size()); p++) {
    if (firstRows[p] <= row) page = p;
  }
  return page;
}

}  // namespace pocketbgg

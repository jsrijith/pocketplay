#include "pocketbgg_list.h"

#include <algorithm>
#include <cstdio>

namespace pocketbgg {

const char* statusLabel(uint16_t status) {
  static const struct {
    uint16_t flag;
    const char* label;
  } ORDER[] = {
      {kOwn, "OWNED"},           {kPreordered, "PREORDERED"}, {kForTrade, "FOR TRADE"}, {kWishlist, "WISHLIST"},
      {kWantToBuy, "WANT TO BUY"}, {kWantToPlay, "WANT TO PLAY"}, {kWant, "WANT"},        {kPrevOwned, "PREV. OWNED"},
  };
  for (const auto& entry : ORDER) {
    if (status & entry.flag) return entry.label;
  }
  return "";
}

void describe(const Game& game, char* out, size_t size) {
  char text[64];
  const char* noun = game.plays == 1 ? "PLAY" : "PLAYS";
  if (game.year == kNoYear) {
    std::snprintf(text, sizeof(text), "%u %s", static_cast<unsigned>(game.plays), noun);
  } else if (game.year < 0) {
    std::snprintf(text, sizeof(text), "%d BC  -  %u %s", -game.year, static_cast<unsigned>(game.plays), noun);
  } else {
    std::snprintf(text, sizeof(text), "%d  -  %u %s", game.year, static_cast<unsigned>(game.plays), noun);
  }
  const char* label = statusLabel(game.status);
  if (label[0] == '\0') {
    std::snprintf(out, size, "%s", text);
  } else {
    std::snprintf(out, size, "%s  -  %s", text, label);
  }
}

void sortGames(std::vector<Game>& games) {
  const auto key = [](const Game& g) {
    std::string k = g.sortSkip < g.name.size() ? g.name.substr(g.sortSkip) : g.name;
    for (char& c : k) {
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return k;
  };
  std::stable_sort(games.begin(), games.end(), [&](const Game& a, const Game& b) {
    const std::string ka = key(a), kb = key(b);
    if (ka != kb) return ka < kb;
    return a.year < b.year;
  });
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

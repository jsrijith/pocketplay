#include "pocketbgg_list.h"

#include <algorithm>
#include <cstdio>

namespace pocketbgg {

const StatusName kStatusNames[kStatusCount] = {
    {kOwn, "OWNED"},           {kPreordered, "PREORDERED"}, {kForTrade, "FOR TRADE"}, {kWishlist, "WISHLIST"},
    {kWantToBuy, "WANT TO BUY"}, {kWantToPlay, "WANT TO PLAY"}, {kWant, "WANT"},        {kPrevOwned, "PREV. OWNED"},
};

const char* statusLabel(uint16_t status) {
  for (const StatusName& entry : kStatusNames) {
    if (status & entry.flag) return entry.label;
  }
  return "";
}

void describe(const Game& game, char* out, size_t size, bool withYear) {
  char text[64];
  const char* noun = game.plays == 1 ? "PLAY" : "PLAYS";
  if (!withYear || game.year == kNoYear) {
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

void formatDate(uint32_t date, bool shortForm, char* out, size_t size) {
  static const char* const MONTHS[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  const unsigned year = date / 10000, month = date / 100 % 100, day = date % 100;
  if (date == 0 || month < 1 || month > 12) {
    if (size > 0) out[0] = '\0';
    return;
  }
  if (shortForm) {
    std::snprintf(out, size, "%s %u", MONTHS[month - 1], year);
  } else {
    std::snprintf(out, size, "%u %s %u", day, MONTHS[month - 1], year);
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

int stepRow(int row, int by, int count) {
  if (row < 0 || row >= count) return -1;
  const int next = row + by;
  return next >= 0 && next < count ? next : -1;
}

}  // namespace pocketbgg

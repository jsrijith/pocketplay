// PocketBGG with no panel: the year and plays line, and the list's pages.
#include <cstdio>
#include <cstring>
#include <vector>

#include "pocketbgg_list.h"

using namespace pocketbgg;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

Game make(int16_t year, uint16_t plays, uint16_t status = 0) {
  Game g;
  g.name = "A";
  g.year = year;
  g.plays = plays;
  g.status = status;
  return g;
}

bool describes(const Game& game, const char* expected, bool withYear = true) {
  char out[40];
  describe(game, out, sizeof(out), withYear);
  if (std::strcmp(out, expected) == 0) return true;
  std::printf("  got \"%s\", expected \"%s\"\n", out, expected);
  return false;
}

void testDescribe() {
  check(describes(make(2020, 33), "2020  -  33 PLAYS"), "a year and plays");
  check(describes(make(2019, 1), "2019  -  1 PLAY"), "one play is singular");
  check(describes(make(2017, 0), "2017  -  0 PLAYS"), "no plays yet");
  check(describes(make(-1400, 3), "1400 BC  -  3 PLAYS"), "a year BC");
  check(describes(make(kNoYear, 2), "2 PLAYS"), "no year leaves the year out");
  check(describes(make(2020, 65535), "2020  -  65535 PLAYS"), "the most plays a game can hold");
  check(describes(make(2020, 33, kOwn), "2020  -  33 PLAYS  -  OWNED"), "an owned game says so");
  check(describes(make(kNoYear, 0, kWishlist), "0 PLAYS  -  WISHLIST"), "a wishlist game with no year");
  check(describes(make(2020, 33, kOwn), "33 PLAYS  -  OWNED", false), "a list row leaves the year out");
  check(describes(make(-1400, 1, kPrevOwned), "1 PLAY  -  PREV. OWNED", false), "a year BC too");
}

void testFormatDate() {
  char out[24];
  formatDate(20261117, false, out, sizeof(out));
  check(std::strcmp(out, "17 NOV 2026") == 0, "a long date");
  formatDate(20260105, true, out, sizeof(out));
  check(std::strcmp(out, "JAN 2026") == 0, "a short date");
  formatDate(0, false, out, sizeof(out));
  check(out[0] == '\0', "no date is empty");
}

void testStatus() {
  check(std::strcmp(statusLabel(0), "") == 0, "no status, no label");
  check(std::strcmp(statusLabel(kOwn | kForTrade), "OWNED") == 0, "owned comes before for trade");
  check(std::strcmp(statusLabel(kPrevOwned | kWishlist), "WISHLIST") == 0, "wishlist comes before previously owned");
  check(std::strcmp(statusLabel(kWant | kWantToPlay), "WANT TO PLAY") == 0, "want to play comes before want");
  check(std::strcmp(statusLabel(kPrevOwned), "PREV. OWNED") == 0, "previously owned alone");
}

void testSort() {
  std::vector<Game> games(4);
  games[0].name = "Zeus on the Loose";
  games[1].name = "The Crew";
  games[1].sortSkip = 4;  // BGG's sortindex 5
  games[2].name = "azul";
  games[3].name = "Crew";
  games[3].year = 1990;
  games[1].year = 2019;
  sortGames(games);
  check(games[0].name == "azul" && games[1].name == "Crew" && games[2].name == "The Crew" && games[3].name == "Zeus on the Loose",
        "by name, ignoring case and The, then by year");
}

void testPaginate() {
  // Rows of 100 with a gap of 4 in a room of 310: three rows fit (308), a fourth doesn't.
  const std::vector<int> pages = paginate({100, 100, 100, 100, 100, 100, 100}, 310, 4);
  check(pages == std::vector<int>({0, 3, 6}), "three rows a page, and the last page holds the one left");
  check(pageOf(pages, 0) == 0 && pageOf(pages, 2) == 0 && pageOf(pages, 3) == 1 && pageOf(pages, 6) == 2,
        "each row is found on its page");
  check(stepRow(0, 1, 5) == 1 && stepRow(3, -1, 5) == 2, "a step to the next and the previous game");
  check(stepRow(4, 1, 5) == 0 && stepRow(0, -1, 5) == 4, "past the last comes the first, and before the first the last");
  check(stepRow(1, 1, 2) == 0 && stepRow(0, -1, 2) == 1, "with two, each step goes to the other");
  check(stepRow(0, 1, 1) == -1 && stepRow(0, 1, 0) == -1 && stepRow(-1, 1, 5) == -1, "one game, none, or none shown");

  // A source that hands out a fixed run of values, then repeats the last.
  auto fixed = [](std::vector<uint32_t> values) {
    return [values, i = size_t(0)]() mutable { return values[i < values.size() ? i++ : values.size() - 1]; };
  };
  check(randomRow(0, -1, fixed({12345})) == -1, "no random pick with no games");
  check(randomRow(1, -1, fixed({12345})) == 0 && randomRow(1, 0, fixed({12345})) == 0, "one game is always the pick, even again");
  bool inRange = true, neverAvoided = true, seen[5] = {};
  for (uint32_t r = 0; r < 1000; r++) {
    const int any = randomRow(5, -1, fixed({r * 2654435761u}));
    inRange = inRange && any >= 0 && any < 5;
    if (any >= 0 && any < 5) seen[any] = true;
    const int other = randomRow(5, 2, fixed({r * 2654435761u}));
    neverAvoided = neverAvoided && other != 2 && other >= 0 && other < 5;
  }
  check(inRange && seen[0] && seen[1] && seen[2] && seen[3] && seen[4], "a random pick reaches every game and no further");
  check(neverAvoided, "a second pick is never the last one");
  check(randomRow(5, 9, fixed({0xFFFFFFF0u})) >= 0 && randomRow(5, 9, fixed({0xFFFFFFF0u})) < 5, "a last pick that's no longer shown is ignored");
  check(randomRow(2, 0, fixed({0})) == 1 && randomRow(2, 1, fixed({7})) == 0, "with two games, the other one");

  // 2^32 isn't a multiple of 3, so the top value would give 0 one time too many: it's thrown back.
  check(uniform(3, fixed({0xFFFFFFFFu, 4})) == 1, "a value past the last whole multiple is drawn again");
  check(uniform(3, fixed({0xFFFFFFFEu})) == 0xFFFFFFFEu % 3, "the last value inside it is kept");
  check(uniform(4, fixed({0xFFFFFFFFu})) == 3, "nothing is thrown back when n divides 2^32");
  check(mix(1) != mix(2) && (mix(1) ^ mix(2)) >> 32 != 0, "a one-bit change reaches the top half");

  check(paginate({100, 100, 100}, 308, 4) == std::vector<int>({0}), "rows that exactly fill the room share a page");
  check(paginate({100, 150, 100, 50}, 260, 4) == std::vector<int>({0, 2}), "rows of different heights");
  check(paginate({}, 300, 4) == std::vector<int>({0}), "no rows gives one empty page");
  check(paginate({50, 400, 50}, 300, 4) == std::vector<int>({0, 1, 2}), "a row too tall for the room gets a page of its own");
}

// A collection's rows land on pages in order, each page starting after the last, whatever
// heights the screen measures.
void testManyRows() {
  std::vector<int> heights;
  const int count = 203;
  for (int i = 0; i < count; i++) heights.push_back(70 + (i % 3) * 42);
  const std::vector<int> pages = paginate(heights, 572, 4);
  bool ordered = pages[0] == 0;
  for (size_t p = 1; p < pages.size(); p++) ordered = ordered && pages[p] > pages[p - 1] && pages[p] < count;
  check(ordered && pages.size() > 1, "203 rows split into several pages, each starting after the last");
}

}  // namespace

int main() {
  testDescribe();
  testStatus();
  testFormatDate();
  testSort();
  testPaginate();
  testManyRows();
  if (failures) {
    std::printf("pocketbgg: %d failed\n", failures);
    return 1;
  }
  std::printf("pocketbgg: all passed\n");
  return 0;
}

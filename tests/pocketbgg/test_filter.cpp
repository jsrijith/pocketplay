// PocketBGG's filters: which games match, the guard that keeps a status chosen, the player
// count steps, and the saved filter.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "pocketbgg_filter.h"

using namespace pocketbgg;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

Game game(uint16_t status, uint16_t plays, uint8_t minPlayers, uint8_t maxPlayers) {
  Game g;
  g.id = 1;
  g.name = "G";
  g.status = status;
  g.plays = plays;
  g.minPlayers = minPlayers;
  g.maxPlayers = maxPlayers;
  return g;
}

void testDefault() {
  const Filter f;
  check(f.statuses == kOwn && !f.playedOnly && f.players == kAnyPlayers, "owned only, any plays, any players");
  check(matches(game(kOwn, 0, 2, 4), f), "an owned game shows");
  check(!matches(game(kWishlist, 0, 2, 4), f), "a wishlist game doesn't");
  check(matches(game(kOwn | kForTrade, 0, 2, 4), f), "owned and for trade still shows");
  check(!matches(game(0, 3, 2, 4), f), "a game with no status doesn't");
}

void testStatuses() {
  Filter f;
  f.statuses = kOwn | kWishlist;
  check(matches(game(kOwn, 0, 0, 0), f) && matches(game(kWishlist, 0, 0, 0), f), "either chosen status shows");
  check(!matches(game(kPrevOwned, 0, 0, 0), f), "one not chosen doesn't");
  f.statuses = kAllStatuses;
  check(matches(game(kPrevOwned, 0, 0, 0), f) && matches(game(kWantToPlay, 0, 0, 0), f), "all statuses shows any of them");
}

void testPlayed() {
  Filter f;
  f.playedOnly = true;
  check(!matches(game(kOwn, 0, 2, 4), f), "played only hides 0 plays");
  check(matches(game(kOwn, 1, 2, 4), f), "and shows 1");
}

void testPlayers() {
  Filter f;
  f.players = 2;
  check(matches(game(kOwn, 0, 2, 4), f) && matches(game(kOwn, 0, 1, 2), f), "2 fits 2-4 and 1-2");
  check(!matches(game(kOwn, 0, 3, 6), f) && !matches(game(kOwn, 0, 1, 1), f), "2 doesn't fit 3-6 or solo");
  f.players = 1;
  check(matches(game(kOwn, 0, 1, 1), f) && !matches(game(kOwn, 0, 2, 2), f), "a solo game for 1, not a 2-player one");
  f.players = kMostPlayers;
  check(matches(game(kOwn, 0, 4, 10), f) && matches(game(kOwn, 0, 2, 8), f), "8+ fits games up to 8 or more");
  check(!matches(game(kOwn, 0, 2, 7), f), "8+ doesn't fit a game up to 7");
  f.players = 4;
  check(matches(game(kOwn, 0, 0, 0), f), "no player count on BGG still shows");
  check(matches(game(kOwn, 0, 2, 0), f), "nor does a missing maximum hide it");
  f.playedOnly = true;
  f.statuses = kWishlist;
  check(!matches(game(kOwn, 5, 2, 4), f), "every test must pass: here the status fails");
}

void testHelpers() {
  std::vector<Game> games = {game(kOwn, 0, 2, 4), game(kWishlist, 0, 2, 4), game(kOwn, 3, 2, 4), game(kOwn | kForTrade, 1, 1, 2)};
  check(visibleGames(games, Filter{}) == std::vector<int>({0, 2, 3}), "the owned games' positions, in order");
  check(countWithStatus(games, kOwn) == 3 && countWithStatus(games, kForTrade) == 1 && countWithStatus(games, kWant) == 0,
        "counts for the filter screen");
  check(visibleGames({}, Filter{}).empty(), "no games, nothing visible");

  Filter f;
  check(!toggleStatus(f, kOwn) && f.statuses == kOwn, "the last status can't be turned off");
  check(toggleStatus(f, kWishlist) && f.statuses == (kOwn | kWishlist), "another can be turned on");
  check(toggleStatus(f, kOwn) && f.statuses == kWishlist, "and then the first off");

  std::vector<Game> dated = {game(kOwn, 1, 0, 0), game(kOwn, 0, 0, 0), game(kOwn, 3, 0, 0), game(kOwn, 2, 0, 0), game(kWishlist, 1, 0, 0)};
  dated[0].lastPlayed = 20250101;
  dated[2].lastPlayed = 20261117;
  dated[3].lastPlayed = 20250101;
  dated[4].lastPlayed = 20270101;
  Filter byDate;
  byDate.order = Order::LastPlayed;
  check(visibleGames(dated, byDate) == std::vector<int>({2, 0, 3, 1}),
        "last played first, a tie kept in A-Z order, never played last, and the filter still applies");
  check(std::strcmp(orderLabel(Order::Name), "A-Z") == 0 && std::strcmp(orderLabel(Order::LastPlayed), "LAST PLAYED") == 0, "order names");

  uint8_t p = kAnyPlayers;
  char label[8];
  std::string labels;
  for (int i = 0; i < 10; i++) {
    playersLabel(p, label, sizeof(label));
    labels += label;
    labels += ' ';
    p = nextPlayers(p);
  }
  check(labels == "ANY 1 2 3 4 5 6 7 8+ ANY ", "the player counts step round");
}

void testSave() {
  Filter f;
  f.statuses = kOwn | kPrevOwned;
  f.playedOnly = true;
  f.players = kMostPlayers;
  f.order = Order::LastPlayed;
  uint8_t data[kFilterBytes];
  check(packFilter(f, data) == kFilterBytes, "5 bytes");
  Filter back;
  check(unpackFilter(kFilterVersion, data, kFilterBytes, back) && back.statuses == f.statuses && back.playedOnly && back.players == kMostPlayers &&
            back.order == Order::LastPlayed,
        "the filter round-trips");
  Filter old;
  old.order = Order::LastPlayed;
  check(unpackFilter(1, data, 4, old) && old.order == Order::Name && old.players == kMostPlayers, "a version 1 filter loads, as A-Z");

  Filter kept;
  kept.players = 3;
  uint8_t bad[kFilterBytes];
  std::memcpy(bad, data, sizeof(bad));
  check(!unpackFilter(kFilterVersion + 1, bad, kFilterBytes, kept), "another version");
  check(!unpackFilter(kFilterVersion, bad, kFilterBytes - 1, kept), "a short save");
  bad[0] = bad[1] = 0;
  check(!unpackFilter(kFilterVersion, bad, kFilterBytes, kept), "no status at all");
  std::memcpy(bad, data, sizeof(bad));
  bad[1] = 1;
  check(!unpackFilter(kFilterVersion, bad, kFilterBytes, kept), "a status that doesn't exist");
  std::memcpy(bad, data, sizeof(bad));
  bad[2] = 2;
  check(!unpackFilter(kFilterVersion, bad, kFilterBytes, kept), "played only that isn't 0 or 1");
  std::memcpy(bad, data, sizeof(bad));
  bad[3] = kMostPlayers + 1;
  check(!unpackFilter(kFilterVersion, bad, kFilterBytes, kept), "a player count past 8+");
  std::memcpy(bad, data, sizeof(bad));
  bad[4] = 2;
  check(!unpackFilter(kFilterVersion, bad, kFilterBytes, kept), "an order that doesn't exist");
  check(!unpackFilter(1, data, kFilterBytes, kept), "version 1 with 5 bytes");
  check(kept.players == 3 && kept.statuses == kOwn, "a refused save leaves the filter alone");
}

}  // namespace

int main() {
  testDefault();
  testStatuses();
  testPlayed();
  testPlayers();
  testHelpers();
  testSave();
  if (failures) {
    std::printf("pocketbgg filter: %d failed\n", failures);
    return 1;
  }
  std::printf("pocketbgg filter: all passed\n");
  return 0;
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// A collection of games, and how it splits into pages. No screen or card, so it can be tested
// off the device: the screen measures each row and hands the heights in.
namespace pocketbgg {

constexpr int16_t kNoYear = INT16_MIN;  // BGG sends no year for some games

// A game's place in the collection, as BGG's <status> flags. A game can have several.
enum Status : uint16_t {
  kOwn = 1 << 0,
  kPrevOwned = 1 << 1,
  kForTrade = 1 << 2,
  kWant = 1 << 3,
  kWantToPlay = 1 << 4,
  kWantToBuy = 1 << 5,
  kWishlist = 1 << 6,
  kPreordered = 1 << 7,
};

struct Game {
  uint32_t id = 0;       // BGG's objectid for the game
  uint32_t collId = 0;   // this copy's id in the collection, which a delta sync matches on
  std::string name;      // ASCII: PocketPlay's font draws nothing else
  uint8_t sortSkip = 0;  // leading characters to skip when sorting, such as "The " in The Crew
  int16_t year = kNoYear;  // negative is BC, as BGG has Nine Men's Morris at -1400
  uint16_t plays = 0;
  uint8_t minPlayers = 0;  // 0 when BGG doesn't say
  uint8_t maxPlayers = 0;
  uint16_t playTime = 0;   // minutes, 0 when BGG doesn't say
  uint16_t status = 0;     // Status flags
  std::string thumbnail;   // its URL, for M6
  std::string lastModified;  // "YYYY-MM-DD HH:MM:SS", as BGG sends it
  uint32_t lastPlayed = 0;   // the latest logged play as YYYYMMDD, or 0 for none
};

// Every status, in the order a game's label picks from and the filter screen lists them.
struct StatusName {
  uint16_t flag;
  const char* label;
};
constexpr int kStatusCount = 8;
extern const StatusName kStatusNames[kStatusCount];

// The status to show for a game: the first of owned, preordered, for trade, wishlist, want to
// buy, want to play, want, previously owned. Empty if none.
const char* statusLabel(uint16_t status);

// The line under a game's name, such as "2020  -  33 PLAYS  -  OWNED", "1400 BC  -  3 PLAYS",
// or "1 PLAY" when there's no year and no status. Without `withYear`, as the list's rows have
// it, "33 PLAYS  -  OWNED", leaving room for the last-played date.
void describe(const Game& game, char* out, size_t size, bool withYear = true);

// By name, ignoring case and the leading characters BGG says to skip, then by year, so the list
// reads as BGG's collection page does.
void sortGames(std::vector<Game>& games);

// A YYYYMMDD date as "12 NOV 2026", or "NOV 2026" with `shortForm`. Empty for 0.
void formatDate(uint32_t date, bool shortForm, char* out, size_t size);

// Splits rows of the given heights into pages, each holding as many whole rows as fit in `room`
// with `gap` between them. Returns the first row of each page. A row taller than the room gets
// a page of its own, so every row is on some page. No rows gives one empty page.
std::vector<int> paginate(const std::vector<int>& heights, int room, int gap);

// Which page a row is on, given paginate()'s answer.
int pageOf(const std::vector<int>& firstRows, int row);

}  // namespace pocketbgg

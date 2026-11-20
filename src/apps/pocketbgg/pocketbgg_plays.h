#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "pocketbgg_list.h"

// The player's logged plays, from /xmlapi2/plays, read for each game's last-played date. No
// screen, card or network, so it's tested off the device.
namespace pocketbgg {

constexpr int kPlaysPerPage = 100;  // BGG's page size for plays

enum class PlaysResult { Ok, BgError, NotPlays, Incomplete };

struct PlaysPage {
  uint32_t total = 0;  // plays BGG holds for the user, across every page
  struct Play {
    uint32_t gameId;
    uint32_t date;  // YYYYMMDD
  };
  std::vector<Play> plays;  // as BGG lists them, newest first; plays with no date are left out
  std::string error;        // BGG's message, for BgError
};

// One page of plays. Fills `out` for Ok, and only `out.error` for BgError.
PlaysResult parsePlaysPage(const char* xml, size_t size, PlaysPage& out);

// How many pages `total` plays come on.
int playsPages(uint32_t total);

// "2026-11-17" as 20261117, or 0 if it isn't a real-looking date.
uint32_t parseDate(const std::string& text);

// Each game's latest play date so far, by game id: a later date replaces an earlier one, so the
// order pages arrive in doesn't matter.
using LastPlayed = std::map<uint32_t, uint32_t>;
void addPlays(const PlaysPage& page, LastPlayed& latest);

// Sets each game's lastPlayed from `latest`. With `complete`, every play has been read, so a game
// with none gets 0; otherwise a game not seen yet keeps the date it had.
void applyLastPlayed(std::vector<Game>& games, const LastPlayed& latest, bool complete);

// Gives each game in `fresh` the lastPlayed its namesake had in `old`, matched by game id, so a
// new sync shows the dates it had until the plays have been read again.
void carryLastPlayed(std::vector<Game>& fresh, const std::vector<Game>& old);

}  // namespace pocketbgg

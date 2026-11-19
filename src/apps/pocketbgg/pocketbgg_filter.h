#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "pocketbgg_list.h"

// Which games the list shows. No screen or card, so it can be tested off the device.
namespace pocketbgg {

constexpr const char* kFilterSave = "pocketbgg-filter";
constexpr uint8_t kFilterVersion = 1;
constexpr size_t kFilterBytes = 4;
constexpr uint8_t kAnyPlayers = 0;
constexpr uint8_t kMostPlayers = 8;  // the last choice, meaning 8 or more
constexpr uint16_t kAllStatuses = 0xFF;

struct Filter {
  uint16_t statuses = kOwn;  // a game shows if it has any of these; never none
  bool playedOnly = false;
  uint8_t players = kAnyPlayers;  // 0 for any, 1 to 7, or kMostPlayers for 8 or more
};

// All three must hold: one of the chosen statuses, a play if playedOnly, and a player count the
// game takes. A game BGG gives no player count for passes that test, so it never vanishes for
// want of data.
bool matches(const Game& game, const Filter& filter);

// The positions in `games` of those that match, in order.
std::vector<int> visibleGames(const std::vector<Game>& games, const Filter& filter);

// How many games have this status, for the filter screen.
int countWithStatus(const std::vector<Game>& games, uint16_t flag);

// Turns a status on or off. False, and nothing changes, for turning off the last one: with no
// status chosen the list could only ever be empty.
bool toggleStatus(Filter& filter, uint16_t flag);

// Any, 1, 2 ... 7, 8 or more, then any again.
uint8_t nextPlayers(uint8_t players);

// "ANY", "4" or "8+".
void playersLabel(uint8_t players, char* out, size_t size);

// As 4 bytes: the statuses (2, low byte first), played only, players.
size_t packFilter(const Filter& filter, uint8_t* out);

// False, leaving `out` alone, unless every value is one the filter screen could have set.
bool unpackFilter(uint8_t version, const uint8_t* in, size_t size, Filter& out);

}  // namespace pocketbgg

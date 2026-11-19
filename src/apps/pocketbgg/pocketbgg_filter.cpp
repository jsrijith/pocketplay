#include "pocketbgg_filter.h"

#include <algorithm>
#include <cstdio>

namespace pocketbgg {

bool matches(const Game& game, const Filter& filter) {
  if ((game.status & filter.statuses) == 0) return false;
  if (filter.playedOnly && game.plays == 0) return false;
  if (filter.players == kAnyPlayers) return true;
  if (game.minPlayers == 0 || game.maxPlayers == 0) return true;  // BGG didn't say
  if (filter.players == kMostPlayers) return game.maxPlayers >= kMostPlayers;
  return game.minPlayers <= filter.players && filter.players <= game.maxPlayers;
}

std::vector<int> visibleGames(const std::vector<Game>& games, const Filter& filter) {
  std::vector<int> visible;
  for (int i = 0; i < static_cast<int>(games.size()); i++) {
    if (matches(games[i], filter)) visible.push_back(i);
  }
  if (filter.order == Order::LastPlayed) {
    // Stable, so games played on the same day, and those never played, stay A to Z.
    std::stable_sort(visible.begin(), visible.end(), [&](int a, int b) { return games[a].lastPlayed > games[b].lastPlayed; });
  }
  return visible;
}

int countWithStatus(const std::vector<Game>& games, uint16_t flag) {
  int count = 0;
  for (const Game& g : games) {
    if (g.status & flag) count++;
  }
  return count;
}

bool toggleStatus(Filter& filter, uint16_t flag) {
  const uint16_t next = static_cast<uint16_t>(filter.statuses ^ flag);
  if ((next & kAllStatuses) == 0) return false;
  filter.statuses = next;
  return true;
}

uint8_t nextPlayers(uint8_t players) { return players >= kMostPlayers ? kAnyPlayers : static_cast<uint8_t>(players + 1); }

void playersLabel(uint8_t players, char* out, size_t size) {
  if (players == kAnyPlayers) {
    std::snprintf(out, size, "ANY");
  } else if (players >= kMostPlayers) {
    std::snprintf(out, size, "%d+", kMostPlayers);
  } else {
    std::snprintf(out, size, "%d", players);
  }
}

const char* orderLabel(Order order) { return order == Order::LastPlayed ? "LAST PLAYED" : "A-Z"; }

size_t packFilter(const Filter& filter, uint8_t* out) {
  out[0] = static_cast<uint8_t>(filter.statuses);
  out[1] = static_cast<uint8_t>(filter.statuses >> 8);
  out[2] = filter.playedOnly ? 1 : 0;
  out[3] = filter.players;
  out[4] = static_cast<uint8_t>(filter.order);
  return kFilterBytes;
}

bool unpackFilter(uint8_t version, const uint8_t* in, size_t size, Filter& out) {
  const bool v1 = version == 1 && size == 4;
  if (!v1 && (version != kFilterVersion || size != kFilterBytes)) return false;
  const uint16_t statuses = static_cast<uint16_t>(in[0] | (in[1] << 8));
  if (statuses == 0 || (statuses & ~kAllStatuses) != 0) return false;
  if (in[2] > 1 || in[3] > kMostPlayers) return false;
  const uint8_t order = v1 ? 0 : in[4];
  if (order > static_cast<uint8_t>(Order::LastPlayed)) return false;
  out.statuses = statuses;
  out.order = static_cast<Order>(order);
  out.playedOnly = in[2] == 1;
  out.players = in[3];
  return true;
}

}  // namespace pocketbgg

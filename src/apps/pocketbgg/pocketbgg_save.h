#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "pocketbgg_list.h"

// The collection as bytes, so it's on the card to browse offline. No screen or card here, so
// it's tested off the device. Packed field by field, never by copying a Game: its strings are
// pointers into memory a restart throws away, and the compiler pads between fields.
//
// Version 1, numbers low byte first:
//   1 byte   version
//   4 bytes  when it was synced, seconds since 1970 UTC (0 if the clock wasn't set)
//   2 bytes  how many games
//   then each game:
//     4 id, 4 collId, 1 sortSkip, 2 year (as int16), 2 plays, 1 minPlayers, 1 maxPlayers,
//     2 playTime, 2 status,
//     then name, thumbnail and lastModified, each as a 2-byte length and its bytes
namespace pocketbgg {

constexpr const char* kListSave = "pocketbgg-list";
constexpr uint8_t kListVersion = 1;
constexpr size_t kMaxGames = 5000;
constexpr size_t kMaxName = 512;
constexpr size_t kMaxThumbnail = 2048;
constexpr size_t kMaxModified = 64;

struct SavedList {
  uint32_t syncedAt = 0;
  std::vector<Game> games;
};

// False, leaving `out` alone, if the list can't be saved as it is: too many games, or a text
// field too long or not plain ASCII.
bool packList(const SavedList& list, std::vector<uint8_t>& out);

// False, leaving `out` alone, unless the bytes are exactly a version 1 list: every length
// inside the data, every text plain ASCII, no status flags that don't exist, and nothing left
// over at the end.
bool unpackList(const uint8_t* data, size_t size, SavedList& out);

}  // namespace pocketbgg

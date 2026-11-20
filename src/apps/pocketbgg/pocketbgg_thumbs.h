#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pocketbgg_image.h"
#include "pocketbgg_list.h"

// Each game's cover, saved on the card already dithered, as the blob "pocketbgg-t<game>-<pic>".
// The pic is the number in the thumbnail's URL, which BGG changes when a game's cover changes,
// so one listing of the save folder says which covers are there and which are out of date,
// with no index file to keep in step. No card or screen code here, so it's tested on the PC.
namespace pocketbgg {

constexpr const char* kThumbPrefix = "pocketbgg-t";
constexpr uint8_t kThumbVersion = 1;
constexpr int kMaxThumbSide = 400;

// The number in ".../pic5575798.jpg", or 0 if the URL has none.
uint32_t picId(const std::string& url);

// "pocketbgg-t102750-5575798", the blob's name.
std::string thumbName(uint32_t gameId, uint32_t pic);

struct SavedThumb {
  uint32_t gameId = 0;
  uint32_t pic = 0;
};

// A file in the save folder, "pocketbgg-t102750-5575798.bin". False for any other file,
// including a ".bit" left over from a save cut off partway.
bool parseThumbFile(const std::string& file, SavedThumb& out);

// The games whose cover to fetch: positions in `games`, one per game id, for every game with
// a picture whose current cover isn't saved.
std::vector<int> thumbsToFetch(const std::vector<Game>& games, const std::vector<SavedThumb>& saved);

// The saved covers no game shows any more: its game left the collection, or its cover changed.
std::vector<SavedThumb> staleThumbs(const std::vector<Game>& games, const std::vector<SavedThumb>& saved);

// A version byte, the width and height as 16 bits each, then the bits.
void packPicture(const Picture& picture, std::vector<uint8_t>& out);
// False, leaving `out` alone, unless the bytes are exactly a picture of a sensible size.
bool unpackPicture(const uint8_t* data, size_t size, Picture& out);

}  // namespace pocketbgg

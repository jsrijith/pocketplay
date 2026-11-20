#include "pocketbgg_thumbs.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <utility>

namespace pocketbgg {

namespace {

// Digits from `at` as a number, up to 9 of them so it can't overflow. False for none, or more.
bool readNumber(const std::string& s, size_t& at, uint32_t& out) {
  const size_t start = at;
  uint32_t value = 0;
  while (at < s.size() && s[at] >= '0' && s[at] <= '9') {
    if (at - start == 9) return false;
    value = value * 10 + static_cast<uint32_t>(s[at] - '0');
    at++;
  }
  out = value;
  return at > start;
}

// The current cover of each game id with one.
std::map<uint32_t, uint32_t> currentCovers(const std::vector<Game>& games) {
  std::map<uint32_t, uint32_t> covers;
  for (const Game& game : games) {
    const uint32_t pic = picId(game.thumbnail);
    if (pic != 0) covers[game.id] = pic;
  }
  return covers;
}

}  // namespace

uint32_t picId(const std::string& url) {
  // The last "/pic" in the URL, followed by digits and then the extension's dot.
  const size_t found = url.rfind("/pic");
  if (found == std::string::npos) return 0;
  size_t at = found + 4;
  uint32_t value = 0;
  if (!readNumber(url, at, value) || at >= url.size() || url[at] != '.') return 0;
  return value;
}

std::string thumbName(uint32_t gameId, uint32_t pic) {
  char name[48];
  std::snprintf(name, sizeof(name), "%s%u-%u", kThumbPrefix, static_cast<unsigned>(gameId), static_cast<unsigned>(pic));
  return name;
}

bool parseThumbFile(const std::string& file, SavedThumb& out) {
  const size_t prefix = std::strlen(kThumbPrefix);
  if (file.compare(0, prefix, kThumbPrefix) != 0) return false;
  size_t at = prefix;
  SavedThumb thumb;
  if (!readNumber(file, at, thumb.gameId) || at >= file.size() || file[at] != '-') return false;
  at++;
  if (!readNumber(file, at, thumb.pic) || file.compare(at, std::string::npos, ".bin") != 0) return false;
  if (thumb.gameId == 0 || thumb.pic == 0) return false;
  out = thumb;
  return true;
}

std::vector<int> thumbsToFetch(const std::vector<Game>& games, const std::vector<SavedThumb>& saved) {
  std::set<std::pair<uint32_t, uint32_t>> have;
  for (const SavedThumb& thumb : saved) have.insert({thumb.gameId, thumb.pic});
  std::set<uint32_t> queued;
  std::vector<int> fetch;
  for (size_t i = 0; i < games.size(); i++) {
    const uint32_t pic = picId(games[i].thumbnail);
    if (pic == 0 || have.count({games[i].id, pic}) != 0) continue;
    if (!queued.insert(games[i].id).second) continue;  // a second copy of the same game
    fetch.push_back(static_cast<int>(i));
  }
  return fetch;
}

std::vector<SavedThumb> staleThumbs(const std::vector<Game>& games, const std::vector<SavedThumb>& saved) {
  const std::map<uint32_t, uint32_t> covers = currentCovers(games);
  std::vector<SavedThumb> stale;
  for (const SavedThumb& thumb : saved) {
    const auto found = covers.find(thumb.gameId);
    if (found == covers.end() || found->second != thumb.pic) stale.push_back(thumb);
  }
  return stale;
}

void packPicture(const Picture& picture, std::vector<uint8_t>& out) {
  out.clear();
  out.reserve(5 + picture.bits.size());
  out.push_back(kThumbVersion);
  out.push_back(static_cast<uint8_t>(picture.width & 0xFF));
  out.push_back(static_cast<uint8_t>(picture.width >> 8));
  out.push_back(static_cast<uint8_t>(picture.height & 0xFF));
  out.push_back(static_cast<uint8_t>(picture.height >> 8));
  out.insert(out.end(), picture.bits.begin(), picture.bits.end());
}

bool unpackPicture(const uint8_t* data, size_t size, Picture& out) {
  if (data == nullptr || size < 5 || data[0] != kThumbVersion) return false;
  const int width = data[1] | (data[2] << 8);
  const int height = data[3] | (data[4] << 8);
  if (width < 1 || height < 1 || width > kMaxThumbSide || height > kMaxThumbSide) return false;
  const size_t bytes = static_cast<size_t>((width + 7) / 8) * height;
  if (size != 5 + bytes) return false;
  Picture picture;
  picture.width = width;
  picture.height = height;
  picture.bits.assign(data + 5, data + size);
  out = std::move(picture);
  return true;
}

}  // namespace pocketbgg

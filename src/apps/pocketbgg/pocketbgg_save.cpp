#include "pocketbgg_save.h"

#include <string>

namespace pocketbgg {

namespace {

constexpr uint16_t kKnownStatus = 0xFF;  // the eight flags in Status

bool validDate(uint32_t date) {
  if (date == 0) return true;
  const uint32_t year = date / 10000, month = date / 100 % 100, day = date % 100;
  return year >= 1900 && year <= 2200 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

bool plainAscii(const std::string& text) {
  for (char c : text) {
    if (c < 0x20 || c > 0x7E) return false;
  }
  return true;
}

class Writer {
 public:
  explicit Writer(std::vector<uint8_t>& out) : out_(out) {}
  void u8(uint8_t v) { out_.push_back(v); }
  void u16(uint16_t v) {
    u8(static_cast<uint8_t>(v));
    u8(static_cast<uint8_t>(v >> 8));
  }
  void u32(uint32_t v) {
    u16(static_cast<uint16_t>(v));
    u16(static_cast<uint16_t>(v >> 16));
  }
  void text(const std::string& s) {
    u16(static_cast<uint16_t>(s.size()));
    out_.insert(out_.end(), s.begin(), s.end());
  }

 private:
  std::vector<uint8_t>& out_;
};

// Reads forward and stops for good at the first read past the end, so one check at the end
// covers them all.
class Reader {
 public:
  Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {}
  bool ok() const { return ok_; }
  bool atEnd() const { return at_ == size_; }
  uint8_t u8() {
    if (!ok_ || at_ + 1 > size_) return fail();
    return data_[at_++];
  }
  uint16_t u16() {
    const uint16_t low = u8();
    return static_cast<uint16_t>(low | (u8() << 8));
  }
  uint32_t u32() {
    const uint32_t low = u16();
    return low | (static_cast<uint32_t>(u16()) << 16);
  }
  bool text(std::string& s, size_t max) {
    const size_t length = u16();
    if (!ok_ || length > max || at_ + length > size_) {
      fail();
      return false;
    }
    s.assign(reinterpret_cast<const char*>(data_ + at_), length);
    at_ += length;
    if (!plainAscii(s)) fail();
    return ok_;
  }

 private:
  uint8_t fail() {
    ok_ = false;
    return 0;
  }
  const uint8_t* data_;
  size_t size_;
  size_t at_ = 0;
  bool ok_ = true;
};

}  // namespace

bool packList(const SavedList& list, std::vector<uint8_t>& out) {
  if (list.games.size() > kMaxGames) return false;
  for (const Game& g : list.games) {
    if (g.name.empty() || g.name.size() > kMaxName || g.thumbnail.size() > kMaxThumbnail || g.lastModified.size() > kMaxModified) {
      return false;
    }
    if (!plainAscii(g.name) || !plainAscii(g.thumbnail) || !plainAscii(g.lastModified) || (g.status & ~kKnownStatus)) return false;
    if (!validDate(g.lastPlayed)) return false;
  }

  std::vector<uint8_t> bytes;
  Writer w(bytes);
  w.u8(kListVersion);
  w.u32(list.syncedAt);
  w.u16(static_cast<uint16_t>(list.games.size()));
  for (const Game& g : list.games) {
    w.u32(g.id);
    w.u32(g.collId);
    w.u8(g.sortSkip);
    w.u16(static_cast<uint16_t>(g.year));
    w.u16(g.plays);
    w.u8(g.minPlayers);
    w.u8(g.maxPlayers);
    w.u16(g.playTime);
    w.u16(g.status);
    w.u32(g.lastPlayed);
    w.text(g.name);
    w.text(g.thumbnail);
    w.text(g.lastModified);
  }
  out.swap(bytes);
  return true;
}

bool unpackList(const uint8_t* data, size_t size, SavedList& out) {
  Reader r(data, size);
  const uint8_t version = r.u8();
  if (version != 1 && version != kListVersion) return false;
  SavedList list;
  list.syncedAt = r.u32();
  const size_t count = r.u16();
  if (!r.ok() || count > kMaxGames) return false;
  list.games.reserve(count);
  for (size_t i = 0; i < count; i++) {
    Game g;
    g.id = r.u32();
    g.collId = r.u32();
    g.sortSkip = r.u8();
    g.year = static_cast<int16_t>(r.u16());
    g.plays = r.u16();
    g.minPlayers = r.u8();
    g.maxPlayers = r.u8();
    g.playTime = r.u16();
    g.status = r.u16();
    if (version >= 2) g.lastPlayed = r.u32();
    if (!r.text(g.name, kMaxName) || !r.text(g.thumbnail, kMaxThumbnail) || !r.text(g.lastModified, kMaxModified)) return false;
    if (g.name.empty() || g.id == 0 || (g.status & ~kKnownStatus) || !validDate(g.lastPlayed)) return false;
    list.games.push_back(std::move(g));
  }
  if (!r.ok() || !r.atEnd()) return false;
  out = std::move(list);
  return true;
}

}  // namespace pocketbgg

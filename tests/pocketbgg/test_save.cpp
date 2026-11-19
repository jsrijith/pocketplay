// PocketBGG's saved collection: the five real games packed and back, and every way a file on the
// card could be damaged refused.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "pocketbgg_collection.h"
#include "pocketbgg_save.h"

using namespace pocketbgg;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

std::vector<Game> sampleGames() {
  std::ifstream file("sample_collection.xml", std::ios::binary);
  std::stringstream text;
  text << file.rdbuf();
  const std::string xml = text.str();
  Collection c;
  parseCollection(xml.data(), xml.size(), c);
  return c.games;
}

bool same(const Game& a, const Game& b) {
  return a.id == b.id && a.collId == b.collId && a.name == b.name && a.sortSkip == b.sortSkip && a.year == b.year &&
         a.plays == b.plays && a.minPlayers == b.minPlayers && a.maxPlayers == b.maxPlayers && a.playTime == b.playTime &&
         a.status == b.status && a.thumbnail == b.thumbnail && a.lastModified == b.lastModified;
}

bool unpack(const std::vector<uint8_t>& bytes, SavedList& out) { return unpackList(bytes.data(), bytes.size(), out); }

void testRoundTrip() {
  SavedList list;
  list.syncedAt = 1795600000;  // November 2026
  list.games = sampleGames();
  check(list.games.size() == 5, "the five sample games");
  std::vector<uint8_t> bytes;
  check(packList(list, bytes), "they pack");
  SavedList back;
  check(unpack(bytes, back) && back.syncedAt == 1795600000 && back.games.size() == 5, "and unpack, with the sync time");
  bool all = back.games.size() == list.games.size();
  for (size_t i = 0; all && i < list.games.size(); i++) all = same(list.games[i], back.games[i]);
  check(all, "every field of every game comes back the same, in the same order");
  check(back.games.size() == 5 && back.games[1].year == -1400, "the year BC survives");
  std::printf("  five games packed into %zu bytes\n", bytes.size());

  SavedList empty;
  check(packList(empty, bytes) && bytes.size() == 7 && unpack(bytes, back) && back.games.empty() && back.syncedAt == 0,
        "an empty collection, 7 bytes");
}

void testRefused() {
  SavedList list;
  list.syncedAt = 42;
  list.games = sampleGames();
  std::vector<uint8_t> good;
  packList(list, good);

  SavedList kept;
  kept.syncedAt = 7;
  // A file cut short anywhere, as by a half-finished write, is refused.
  bool allRefused = true;
  for (size_t length = 0; length < good.size(); length++) {
    const std::vector<uint8_t> cut(good.begin(), good.begin() + static_cast<long>(length));
    if (unpack(cut, kept)) allRefused = false;
  }
  check(allRefused && kept.syncedAt == 7, "every shorter file is refused, leaving the list alone");

  std::vector<uint8_t> bad = good;
  bad.push_back(0);
  check(!unpack(bad, kept), "a byte left over at the end");
  bad = good;
  bad[0] = kListVersion + 1;
  check(!unpack(bad, kept), "another version");
  bad = good;
  bad[5] = 0xFF;  // the count's low byte: far more games than the file holds
  bad[6] = 0xFF;
  check(!unpack(bad, kept), "a count past the limit");

  // The first game starts at byte 7; its status is at 7 + 4 + 4 + 1 + 2 + 2 + 1 + 1 + 2 = 24.
  bad = good;
  bad[25] = 0x01;  // status bit 8, which doesn't exist
  check(!unpack(bad, kept), "a status flag that doesn't exist");
  bad = good;
  bad[7] = bad[8] = bad[9] = bad[10] = 0;
  check(!unpack(bad, kept), "a game with id 0");
  bad = good;
  bad[28] = static_cast<uint8_t>(0xC3);  // the first byte of the first name, after its 2-byte length at 26
  check(!unpack(bad, kept), "a name that isn't plain ASCII");
  bad = good;
  bad[26] = 0xFF;  // the first name's length, far past the end
  bad[27] = 0xFF;
  check(!unpack(bad, kept), "a length running past the end");
  check(kept.syncedAt == 7, "no refused file changes the list");

  SavedList wrong = list;
  wrong.games[0].name = "Pok\xC3\xA9mon";
  std::vector<uint8_t> out{9};
  check(!packList(wrong, out) && out.size() == 1, "a name past ASCII isn't saved");
  wrong = list;
  wrong.games[0].name.clear();
  check(!packList(wrong, out), "a game with no name isn't saved");
  wrong = list;
  wrong.games[0].thumbnail.assign(kMaxThumbnail + 1, 'x');
  check(!packList(wrong, out), "a thumbnail URL too long isn't saved");
  wrong = list;
  wrong.games.resize(kMaxGames + 1, list.games[0]);
  check(!packList(wrong, out), "more games than the limit aren't saved");
}

}  // namespace

int main() {
  testRoundTrip();
  testRefused();
  if (failures) {
    std::printf("pocketbgg save: %d failed\n", failures);
    return 1;
  }
  std::printf("pocketbgg save: all passed\n");
  return 0;
}

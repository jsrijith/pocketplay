// PocketBGG's saved covers: the pic id in a thumbnail URL, the names they're saved under, which
// covers to fetch and which to delete, and packing a picture to save.
#include <cstdio>
#include <string>
#include <vector>

#include "pocketbgg_thumbs.h"

using namespace pocketbgg;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

const char* const URL =
    "https://cf.geekdo-images.com/i31-zA-O2NTo_pxN6xA7hQ__small/img/C-STnVEILe4Xbjd_cpf9cQq_-J0=/fit-in/200x150/"
    "filters:strip_icc()/pic5575798.jpg";

Game game(uint32_t id, const std::string& thumbnail) {
  Game g;
  g.id = id;
  g.name = "G";
  g.thumbnail = thumbnail;
  return g;
}

std::string cover(uint32_t pic) { return "https://cf.geekdo-images.com/x/img/y=/fit-in/200x150/pic" + std::to_string(pic) + ".png"; }

void testPicId() {
  check(picId(URL) == 5575798, "a real thumbnail URL gives its pic id");
  check(picId(cover(42)) == 42, "a PNG's pic id");
  check(picId("") == 0, "no URL, no pic");
  check(picId("https://example.com/image.jpg") == 0, "a URL with no /pic");
  check(picId("https://example.com/pic.jpg") == 0, "/pic with no number");
  check(picId("https://example.com/pic123") == 0, "a number with no extension after it");
  check(picId("https://example.com/pic12345678901.jpg") == 0, "a number too long to hold");
  check(picId("https://example.com/pic1/pic77.jpg") == 77, "the last /pic counts");
}

void testNames() {
  check(thumbName(102750, 5575798) == "pocketbgg-t102750-5575798", "a cover's name");
  check(thumbName(999999999, 999999999).size() <= 44, "the longest name fits the card's path");
  SavedThumb t;
  check(parseThumbFile("pocketbgg-t102750-5575798.bin", t) && t.gameId == 102750 && t.pic == 5575798, "a cover's file");
  check(!parseThumbFile("pocketbgg-t102750-5575798.bit", t), "a cut-off save isn't a cover");
  check(!parseThumbFile("pocketbgg-list.bin", t), "the list isn't a cover");
  check(!parseThumbFile("pocketbgg-t102750.bin", t), "a name with no pic");
  check(!parseThumbFile("pocketbgg-t-5.bin", t), "a name with no game");
  check(!parseThumbFile("pocketbgg-t0-5.bin", t), "game 0 isn't a game");
  check(!parseThumbFile("pocketbgg-t1-2.bin.bak", t), "something after .bin");
  check(!parseThumbFile("wordle.sav", t), "another game's save");
}

void testFetchAndStale() {
  std::vector<Game> games = {game(1, cover(10)), game(2, cover(20)), game(3, ""), game(4, cover(40)), game(1, cover(10))};
  // Game 1's cover saved; game 2's old cover saved; game 9 gone from the collection.
  const std::vector<SavedThumb> saved = {{1, 10}, {2, 19}, {9, 90}};
  const std::vector<int> fetch = thumbsToFetch(games, saved);
  check(fetch.size() == 2 && fetch[0] == 1 && fetch[1] == 3, "fetch the changed cover and the missing one, once each");
  check(thumbsToFetch(games, {}).size() == 3, "with nothing saved, every game with a picture, once each");

  const std::vector<SavedThumb> stale = staleThumbs(games, saved);
  check(stale.size() == 2, "two stale covers");
  check(stale.size() == 2 && stale[0].gameId == 2 && stale[0].pic == 19, "the old cover of a game whose cover changed");
  check(stale.size() == 2 && stale[1].gameId == 9, "the cover of a game no longer in the collection");
  check(staleThumbs({}, saved).size() == 3, "an empty collection leaves no cover");
}

void testPack() {
  Picture p;
  p.width = 10;
  p.height = 3;
  p.bits = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
  std::vector<uint8_t> bytes;
  packPicture(p, bytes);
  check(bytes.size() == 11, "5 header bytes and 6 of bits");
  Picture q;
  check(unpackPicture(bytes.data(), bytes.size(), q), "a packed picture unpacks");
  check(q.width == 10 && q.height == 3 && q.bits == p.bits, "and comes back the same");

  Picture untouched;
  std::vector<uint8_t> bad = bytes;
  bad[0] = 2;
  check(!unpackPicture(bad.data(), bad.size(), untouched), "another version is refused");
  check(!unpackPicture(bytes.data(), bytes.size() - 1, untouched), "a byte short is refused");
  bad = bytes;
  bad.push_back(0);
  check(!unpackPicture(bad.data(), bad.size(), untouched), "a byte over is refused");
  bad = bytes;
  bad[1] = 0;
  bad[2] = 0;
  check(!unpackPicture(bad.data(), bad.size(), untouched), "width 0 is refused");
  bad = bytes;
  bad[1] = 0xFF;
  bad[2] = 0xFF;
  check(!unpackPicture(bad.data(), bad.size(), untouched), "a huge width is refused");
  check(!unpackPicture(nullptr, 0, untouched), "nothing is refused");
  check(untouched.empty(), "a refused picture leaves the old one alone");
}

}  // namespace

int main() {
  testPicId();
  testNames();
  testFetchAndStale();
  testPack();
  if (failures) {
    std::printf("pocketbgg thumbs: %d failed\n", failures);
    return 1;
  }
  std::printf("pocketbgg thumbs: all passed\n");
  return 0;
}

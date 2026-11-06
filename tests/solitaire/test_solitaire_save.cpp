// Solitaire's save packing, its undo history across a save, and the record.
//
// The round trip plays random legal games and checks, at every step, that a board
// packed and unpacked is the same board, and that undoing the restored copy all the
// way walks back through exactly the positions the original passed through.
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "solitaire_record.h"
#include "solitaire_rules.h"
#include "solitaire_save.h"

using namespace solitaire;

namespace {

int checksRun = 0;
int checksFailed = 0;

void check(bool condition, const std::string& what) {
  checksRun++;
  if (condition) return;
  checksFailed++;
  if (checksFailed <= 20) printf("FAIL %s\n", what.c_str());
}

bool sameBoard(const Game& a, const Game& b) {
  if (a.seed() != b.seed() || a.drawThree() != b.drawThree() || a.moveCount() != b.moveCount()) return false;
  for (int p = 0; p < kPileCount; p++) {
    const Pile& x = a.pile(p);
    const Pile& y = b.pile(p);
    if (x.count != y.count || memcmp(x.cards, y.cards, x.count) != 0) return false;
  }
  return true;
}

bool sameHistory(const Game& a, const Game& b) {
  if (a.undoDepth() != b.undoDepth()) return false;
  for (int i = 0; i < a.undoDepth(); i++) {
    const Move& x = a.undoStep(i);
    const Move& y = b.undoStep(i);
    if (x.from != y.from || x.to != y.to || x.count != y.count || x.flipped != y.flipped || x.stock != y.stock) return false;
  }
  return true;
}

// One random legal step: a move if any is legal and the dice say so, else the stock.
bool randomStep(Game& game, std::mt19937& rng) {
  struct Option {
    int from, to, count;
  };
  std::vector<Option> options;
  for (int from = kWastePile; from < kPileCount; from++) {
    for (int to = kFirstFoundation; to < kPileCount; to++) {
      for (int count = 1; count <= game.pile(from).count; count++) {
        if (game.canMove(from, to, count)) options.push_back({from, to, count});
      }
    }
  }
  if (!options.empty() && rng() % 3 != 0) {
    const Option& pick = options[rng() % options.size()];
    return game.move(pick.from, pick.to, pick.count);
  }
  return game.drawFromStock();
}

Game roundTrip(const Game& game, bool& ok) {
  uint8_t data[MAX_SAVE_BYTES];
  const size_t size = packGame(game, data);
  Game back;
  ok = size <= MAX_SAVE_BYTES && unpackGame(data, size, back);
  return back;
}

void testRoundTrips() {
  std::mt19937 rng(12345);
  int games = 0;
  int steps = 0;
  int deepest = 0;
  for (int g = 0; g < 300; g++) {
    Game game;
    game.deal(static_cast<uint32_t>(rng()), g % 2 == 1);
    std::vector<Game> path{game};
    const int length = 20 + static_cast<int>(rng() % 200);
    for (int s = 0; s < length; s++) {
      if (!randomStep(game, rng)) break;
      if (rng() % 10 == 0 && game.canUndo()) {
        // Taking the step straight back returns to the position before it.
        game.undo();
        check(sameBoard(game, path.back()), "game " + std::to_string(g) + " step " + std::to_string(s) + " undoes to the wrong place");
      } else {
        path.push_back(game);
      }
      steps++;

      bool ok = false;
      const Game back = roundTrip(game, ok);
      check(ok, "game " + std::to_string(g) + " step " + std::to_string(s) + " doesn't unpack");
      if (!ok) continue;
      check(sameBoard(game, back), "game " + std::to_string(g) + " step " + std::to_string(s) + " comes back different");
      check(sameHistory(game, back), "game " + std::to_string(g) + " step " + std::to_string(s) + " loses its undo history");
      if (game.undoDepth() > deepest) deepest = game.undoDepth();
    }

    // Undo the restored copy all the way: it walks back through the same positions.
    bool ok = false;
    Game back = roundTrip(game, ok);
    if (!ok) continue;
    int at = static_cast<int>(path.size()) - 1;
    while (back.canUndo()) {
      back.undo();
      at--;
      if (at < 0) break;
      check(sameBoard(back, path[at]), "game " + std::to_string(g) + " undoes to a position it never held");
    }
    // The undo history holds 96 steps, so a longer game stops short of the deal.
    if (static_cast<int>(path.size()) - 1 <= kUndoDepth) check(at == 0 && sameBoard(back, path[0]), "game " + std::to_string(g) + " doesn't undo back to its deal");
    games++;
  }
  printf("%d games, %d steps packed and unpacked, undo history up to %d steps\n", games, steps, deepest);
  check(deepest == kUndoDepth, "no game filled the undo history, so the cap wasn't tested");
}

// A board a few moves in, packed, for the damage tests to break.
std::vector<uint8_t> sampleSave(Game& game) {
  game.deal(77, false);
  std::mt19937 rng(9);
  for (int s = 0; s < 40; s++) randomStep(game, rng);
  std::vector<uint8_t> data(MAX_SAVE_BYTES);
  data.resize(packGame(game, data.data()));
  return data;
}

bool unpacks(const std::vector<uint8_t>& data) {
  Game game;
  return unpackGame(data.data(), data.size(), game);
}

int firstCardOf(const Game& game, int pile) {
  int at = 20;
  for (int p = 0; p < pile; p++) at += game.pile(p).count;
  return at;
}

void testRejects() {
  Game game;
  const std::vector<uint8_t> good = sampleSave(game);
  check(unpacks(good), "the sample save doesn't unpack");
  check(game.undoDepth() > 0, "the sample has no undo history to damage");

  {
    std::vector<uint8_t> data = good;
    data.pop_back();
    check(!unpacks(data), "a short save unpacks");
    data = good;
    data.push_back(0);
    check(!unpacks(data), "a long save unpacks");
    check(!unpacks(std::vector<uint8_t>(10)), "a tiny save unpacks");
  }
  {
    std::vector<uint8_t> data = good;
    data[4] = 2;
    check(!unpacks(data), "a draw mode of 2 unpacks");
  }
  {
    std::vector<uint8_t> data = good;
    data[7 + kFirstTableau] = kPileCapacity + 1;
    check(!unpacks(data), "a pile past its capacity unpacks");
  }
  {
    // Move one card's count from a pile to its neighbour: still 52, but the faces or
    // the deck no longer fit, or the total changes.
    std::vector<uint8_t> data = good;
    data[7 + kFirstTableau + 6]++;
    check(!unpacks(data), "a deck of 53 unpacks");
  }
  {
    std::vector<uint8_t> data = good;
    const int a = firstCardOf(game, kFirstTableau + 6);
    data[a] = data[a + 1];
    check(!unpacks(data), "a repeated card unpacks");
  }
  {
    std::vector<uint8_t> data = good;
    data[firstCardOf(game, kFirstTableau)] |= 0x40;
    check(!unpacks(data), "a card with the spare bit set unpacks");
  }
  {
    std::vector<uint8_t> data = good;
    const int top = firstCardOf(game, kFirstTableau) + game.pile(kFirstTableau).count - 1;
    data[top] &= 0x7F;
    check(!unpacks(data), "a column with a face-down top unpacks");
  }
  if (game.pile(kStockPile).count > 0) {
    std::vector<uint8_t> data = good;
    data[firstCardOf(game, kStockPile)] |= 0x80;
    check(!unpacks(data), "a face-up stock card unpacks");
  }
  if (game.pile(kWastePile).count > 0) {
    std::vector<uint8_t> data = good;
    data[firstCardOf(game, kWastePile)] &= 0x7F;
    check(!unpacks(data), "a face-down waste card unpacks");
  }
  {
    std::vector<uint8_t> data = good;
    data[SAVE_HEADER_BYTES - 1] = kUndoDepth + 1;
    data.resize(SAVE_HEADER_BYTES + (kUndoDepth + 1) * UNDO_STEP_BYTES);
    check(!unpacks(data), "an undo history past 96 steps unpacks");
  }
  {
    // More undo steps than moves made.
    std::vector<uint8_t> data = good;
    data[5] = 0;
    data[6] = 0;
    check(!unpacks(data), "an undo history longer than the game unpacks");
  }

  // Every single-byte change to the newest undo step: the ones that unpack must
  // still undo to legal positions, which unpackGame has checked by replaying them.
  int refused = 0;
  int accepted = 0;
  const size_t newest = good.size() - UNDO_STEP_BYTES;
  for (size_t offset = newest; offset < good.size(); offset++) {
    for (int value = 0; value < 256; value++) {
      if (value == good[offset]) continue;
      std::vector<uint8_t> data = good;
      data[offset] = static_cast<uint8_t>(value);
      Game back;
      if (!unpackGame(data.data(), data.size(), back)) {
        refused++;
        continue;
      }
      accepted++;
      while (back.canUndo()) back.undo();
    }
  }
  printf("Damaged newest undo step: %d refused, %d still a legal history\n", refused, accepted);
  check(refused > 0, "no damaged undo step was refused");
}

void testRecord() {
  std::vector<uint8_t> results;
  Record empty = summarise(results);
  check(empty.played == 0 && empty.wins == 0 && empty.streak == 0 && empty.recent[15] == 0, "an empty record isn't empty");

  appendResult(results, true);
  appendResult(results, false);
  appendResult(results, true);
  appendResult(results, true);
  Record r = summarise(results);
  check(r.played == 4 && r.wins == 3 && r.streak == 2, "four games summarise wrongly");
  check(r.recent[15] == RESULT_WON && r.recent[13] == RESULT_LOST && r.recent[12] == RESULT_WON && r.recent[11] == 0, "the recent hand is out of order");

  for (size_t i = 0; i < MAX_RESULTS + 10; i++) appendResult(results, i % 2 == 0);
  check(results.size() == MAX_RESULTS, "the record grows past its cap");
  check(results.back() == RESULT_LOST, "the newest game isn't last");  // i = MAX_RESULTS + 9 is odd

  check(resultsValid(results.data(), results.size()), "a real record is refused");
  std::vector<uint8_t> bad = results;
  bad[3] = 0;
  check(!resultsValid(bad.data(), bad.size()), "a record with a 0 passes");
  bad.push_back(RESULT_WON);
  check(!resultsValid(bad.data(), bad.size()), "a record past its cap passes");
}

}  // namespace

int main() {
  testRoundTrips();
  testRejects();
  testRecord();
  printf("%d checks, %d failed\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}

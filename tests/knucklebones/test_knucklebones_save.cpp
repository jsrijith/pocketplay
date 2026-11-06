// Knucklebones' save packing. Whole matches are played, you at random and the
// opponent by its own brain, and packed after every placement: what comes back
// must be the same match, and play must go on from the copy exactly as from the
// original. Damaged saves must be refused.
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

#include "knucklebones_brain.h"
#include "knucklebones_rules.h"
#include "knucklebones_save.h"

using namespace knucklebones;

namespace {

int checksRun = 0;
int checksFailed = 0;

void check(bool condition, const std::string& what) {
  checksRun++;
  if (condition) return;
  checksFailed++;
  if (checksFailed <= 20) printf("FAIL %s\n", what.c_str());
}

bool sameGrid(const Grid& a, const Grid& b) { return memcmp(a.cell, b.cell, sizeof(a.cell)) == 0; }

bool sameGame(const Game& a, const Game& b) {
  return sameGrid(a.grid[0], b.grid[0]) && sameGrid(a.grid[1], b.grid[1]) && a.turn == b.turn && a.die == b.die && a.rng == b.rng;
}

bool roundTrip(const Game& game, Game& back) {
  uint8_t data[MATCH_BYTES];
  packMatch(game, data);
  return unpackMatch(data, sizeof(data), back);
}

void testMatches() {
  std::mt19937 rng(2024);
  int placements = 0;
  int ended = 0;
  for (int match = 0; match < 2000; ++match) {
    Game game{};
    start(game, rng() | 1u);
    Game back{};
    check(roundTrip(game, back) && sameGame(game, back), "a new match doesn't round-trip");
    while (!over(game)) {
      int column = -1;
      if (game.turn == 0) {
        do {
          column = static_cast<int>(rng() % kColumns);
        } while (!canPlace(game, column));
      } else {
        column = chooseColumn(game);
      }
      check(place(game, column), "a legal placement was refused");
      placements++;
      if (over(game)) {
        // A finished match is never saved, so it must not unpack either.
        check(!roundTrip(game, back), "a finished match unpacks");
        ended++;
        break;
      }
      const bool ok = roundTrip(game, back);
      check(ok, "match " + std::to_string(match) + " doesn't unpack");
      if (!ok) break;
      check(sameGame(game, back), "match " + std::to_string(match) + " comes back different");
      // Play on from the copy, so whatever a save loses shows up later.
      game = back;
    }
  }
  printf("%d placements packed and played on from the copy, %d matches ended\n", placements, ended);
}

Game sample() {
  Game game{};
  start(game, 4242);
  // A few placements each, so both grids hold dice.
  for (int i = 0; i < 6; ++i) {
    int column = game.turn == 0 ? i % kColumns : chooseColumn(game);
    while (!canPlace(game, column)) column = (column + 1) % kColumns;
    place(game, column);
  }
  return game;
}

void testRejects() {
  const Game game = sample();
  uint8_t good[MATCH_BYTES];
  packMatch(game, good);
  Game scratch{};
  check(unpackMatch(good, sizeof(good), scratch), "the sample doesn't unpack");
  check(!unpackMatch(good, sizeof(good) - 1, scratch), "a short save unpacks");

  uint8_t data[MATCH_BYTES];
  const auto damaged = [&](size_t at, uint8_t value) {
    memcpy(data, good, sizeof(data));
    data[at] = value;
    return unpackMatch(data, sizeof(data), scratch);
  };
  check(!damaged(18, 2), "a third seat's turn unpacks");
  check(!damaged(19, 0), "no die waiting unpacks");
  check(!damaged(19, 7), "a die of 7 unpacks");
  check(!damaged(0, 7), "a face of 7 on the grid unpacks");
  memcpy(data, good, sizeof(data));
  data[20] = data[21] = data[22] = data[23] = 0;
  check(!unpackMatch(data, sizeof(data), scratch), "a random state of zero unpacks");

  // A die above a gap: column 0's near slot emptied under a die.
  memcpy(data, good, sizeof(data));
  data[0] = 0;
  data[1] = 3;
  check(!unpackMatch(data, sizeof(data), scratch), "a die above a gap unpacks");

  // A full grid ends the match, so a save never holds one.
  memcpy(data, good, sizeof(data));
  for (int i = 0; i < 9; ++i) data[i] = static_cast<uint8_t>(1 + i % kFaces);
  check(!unpackMatch(data, sizeof(data), scratch), "a full grid unpacks");
}

void testRecord() {
  Record record;
  record.wins = 300;
  record.losses = 7;
  record.draws = 1;
  record.hasLast = true;
  const Game game = sample();
  record.lastYours = game.grid[0];
  for (int column = 0; column < kColumns; ++column) {
    for (int row = 0; row < kRows; ++row) record.lastTheirs.cell[column][row] = static_cast<uint8_t>(1 + (column + row) % kFaces);
  }
  uint8_t data[RECORD_BYTES];
  packRecord(record, data);
  Record back;
  check(unpackRecord(data, sizeof(data), back), "a record doesn't unpack");
  check(back.wins == 300 && back.losses == 7 && back.draws == 1 && back.hasLast && sameGrid(back.lastYours, record.lastYours) &&
            sameGrid(back.lastTheirs, record.lastTheirs),
        "a record comes back different");
  check(!unpackRecord(data, sizeof(data) - 1, back), "a short record unpacks");

  uint8_t bad[RECORD_BYTES];
  memcpy(bad, data, sizeof(bad));
  bad[6] = 2;
  check(!unpackRecord(bad, sizeof(bad), back), "a last-match flag of 2 unpacks");
  memcpy(bad, data, sizeof(bad));
  bad[16] = 0;  // the opponent's full grid, now with a gap under a die
  check(!unpackRecord(bad, sizeof(bad), back), "a die above a gap in the record unpacks");
  memcpy(bad, data, sizeof(bad));
  bad[16 + 8] = 0;  // neither grid full: no match ended like that
  check(!unpackRecord(bad, sizeof(bad), back), "last grids with neither full unpack");
  memcpy(bad, data, sizeof(bad));
  bad[0] = bad[1] = bad[2] = bad[3] = bad[4] = bad[5] = 0;
  check(!unpackRecord(bad, sizeof(bad), back), "a last match with nothing tallied unpacks");

  Record none;
  packRecord(none, data);
  check(unpackRecord(data, sizeof(data), back) && !back.hasLast, "an empty record doesn't round-trip");
  data[7] = 4;
  check(!unpackRecord(data, sizeof(data), back), "grids with no match claimed unpack");
}

}  // namespace

int main() {
  testMatches();
  testRejects();
  testRecord();
  printf("%d checks, %d failed\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}

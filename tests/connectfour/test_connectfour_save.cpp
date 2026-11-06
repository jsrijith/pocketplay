// Connect Four's save packing. Whole games are played at random and packed after
// every disc: what comes back must be the same game, and play must go on from the
// copy exactly as from the original. The record is built the way the game builds
// it, from every finished game. Damaged saves must be refused.
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

#include "connectfour_rules.h"
#include "connectfour_save.h"

using namespace connectfour;

namespace {

int checksRun = 0;
int checksFailed = 0;

void check(bool condition, const std::string& what) {
  checksRun++;
  if (condition) return;
  checksFailed++;
  if (checksFailed <= 20) printf("FAIL %s\n", what.c_str());
}

bool sameGame(const Game& a, const Game& b) {
  return memcmp(a.cell, b.cell, sizeof(a.cell)) == 0 && a.lastColumn == b.lastColumn && a.lastRow == b.lastRow && a.turn == b.turn &&
         a.outcome == b.outcome;
}

bool roundTrip(const Game& game, Game& back) {
  uint8_t data[GAME_BYTES];
  packGame(game, data);
  return unpackGame(data, sizeof(data), back);
}

// What the game writes when a game ends, from light's side.
Record recordOf(const Game& game) {
  Record record;
  record.hasLast = true;
  record.lastOutcome = game.outcome == Outcome::Draw ? LAST_DRAW : (game.outcome == Outcome::LightWins ? LAST_WON : LAST_LOST);
  if (game.outcome == Outcome::LightWins) winningLine(game, kLight, record.lastLine);
  if (game.outcome == Outcome::DarkWins) winningLine(game, kDark, record.lastLine);
  for (int column = 0; column < kColumns; ++column) {
    for (int row = 0; row < kRows; ++row) record.lastCells[column * kRows + row] = game.cell[column][row];
  }
  record.wins = record.lastOutcome == LAST_WON ? 1 : 0;
  record.losses = record.lastOutcome == LAST_LOST ? 1 : 0;
  record.draws = record.lastOutcome == LAST_DRAW ? 1 : 0;
  return record;
}

int outcomes[3] = {};

void testGames() {
  std::mt19937 rng(4);
  int discs = 0;
  for (int g = 0; g < 5000; ++g) {
    Game game{};
    start(game);
    Game back{};
    check(roundTrip(game, back) && sameGame(game, back), "a new game doesn't round-trip");
    while (!over(game)) {
      int column = 0;
      do {
        column = static_cast<int>(rng() % kColumns);
      } while (!canDrop(game, column));
      drop(game, column);
      discs++;
      if (over(game)) {
        check(!roundTrip(game, back), "a settled game unpacks");
        break;
      }
      const bool ok = roundTrip(game, back);
      check(ok, "game " + std::to_string(g) + " doesn't unpack");
      if (!ok) break;
      check(sameGame(game, back), "game " + std::to_string(g) + " comes back different");
      game = back;
    }
    const Record record = recordOf(game);
    outcomes[record.lastOutcome]++;
    uint8_t data[RECORD_BYTES];
    packRecord(record, data);
    Record read;
    const bool ok = unpackRecord(data, sizeof(data), read);
    check(ok, "the record of game " + std::to_string(g) + " doesn't unpack");
    check(!ok || (read.lastOutcome == record.lastOutcome && memcmp(read.lastLine, record.lastLine, sizeof(read.lastLine)) == 0 &&
                  memcmp(read.lastCells, record.lastCells, sizeof(read.lastCells)) == 0),
          "the record of game " + std::to_string(g) + " comes back different");
  }
  printf("%d discs packed and played on from the copy; records of %d wins, %d losses, %d draws\n", discs, outcomes[LAST_WON], outcomes[LAST_LOST],
         outcomes[LAST_DRAW]);
  check(outcomes[LAST_DRAW] > 0, "no draw was played, so a drawn record wasn't tested");
}

Game sample() {
  Game game{};
  start(game);
  const int columns[] = {3, 3, 2, 4, 5, 0};
  for (const int column : columns) drop(game, column);
  return game;
}

void testRejects() {
  const Game game = sample();
  uint8_t good[GAME_BYTES];
  packGame(game, good);
  Game scratch{};
  check(unpackGame(good, sizeof(good), scratch), "the sample doesn't unpack");
  check(!unpackGame(good, sizeof(good) - 1, scratch), "a short save unpacks");

  uint8_t data[GAME_BYTES];
  const auto damaged = [&](size_t at, uint8_t value) {
    memcpy(data, good, sizeof(data));
    data[at] = value;
    return unpackGame(data, sizeof(data), scratch);
  };
  check(!damaged(44, 3) && !damaged(44, 0), "a turn other than light or dark unpacks");
  check(!damaged(44, kDark), "the wrong side to move unpacks");
  check(!damaged(3 * kRows + 2, kLight), "an extra disc unpacks");
  check(!damaged(0, 3), "a cell of 3 unpacks");
  check(!damaged(1 * kRows + 1, kLight), "a disc above a gap unpacks");
  check(!damaged(42, kColumns), "a last column past the board unpacks");
  check(!damaged(43, kRows), "a last row past the board unpacks");
  // The sample's last disc is dark's in column 0, row 0.
  check(!damaged(42, 3), "a last disc that isn't on top unpacks");
  memcpy(data, good, sizeof(data));
  data[42] = data[43] = 0xFF;
  check(!unpackGame(data, sizeof(data), scratch), "discs with no last disc unpack");
}

void testRecordRejects() {
  Game game{};
  start(game);
  const int columns[] = {0, 1, 0, 1, 0, 1, 0};  // light's four up column 0
  for (const int column : columns) drop(game, column);
  const Record won = recordOf(game);
  uint8_t good[RECORD_BYTES];
  packRecord(won, good);
  Record scratch;
  check(unpackRecord(good, sizeof(good), scratch), "a won record doesn't unpack");
  check(!unpackRecord(good, sizeof(good) - 1, scratch), "a short record unpacks");

  uint8_t data[RECORD_BYTES];
  const auto damaged = [&](size_t at, uint8_t value) {
    memcpy(data, good, sizeof(data));
    data[at] = value;
    return unpackRecord(data, sizeof(data), scratch);
  };
  check(!damaged(6, 2), "a last-game flag of 2 unpacks");
  check(!damaged(7, 3), "a fourth way to end unpacks");
  check(!damaged(7, LAST_LOST), "a loss on a line of your discs unpacks");
  check(!damaged(7, LAST_DRAW), "a draw on a board that isn't full unpacks");
  check(!damaged(11, 5), "a line that isn't straight unpacks");
  check(!damaged(8, 0xFF), "a win with three cells unpacks");
  check(!damaged(12 + 2 * kRows + 1, kDark), "a disc above a gap in the record unpacks");
  check(!damaged(1, 0), "a win with no wins tallied unpacks");

  Record none;
  packRecord(none, data);
  check(unpackRecord(data, sizeof(data), scratch) && !scratch.hasLast, "an empty record doesn't round-trip");
  data[12] = kLight;
  check(!unpackRecord(data, sizeof(data), scratch), "a board with no game claimed unpacks");
}

}  // namespace

int main() {
  testGames();
  testRejects();
  testRecordRejects();
  printf("%d checks, %d failed\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}

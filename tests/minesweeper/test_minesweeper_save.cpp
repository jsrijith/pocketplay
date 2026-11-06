// Minesweeper's save packing: the board in play and the history. Random games are
// packed after every move and must come back the same, and damaged saves are
// refused rather than played.
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

#include "minesweeper_rules.h"
#include "minesweeper_save.h"

using namespace minesweeper;

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
  return memcmp(a.cell, b.cell, sizeof(a.cell)) == 0 && a.rng == b.rng && a.status == b.status;
}

bool roundTrips(const OpenBoard& board) {
  uint8_t data[BOARD_BYTES];
  packBoard(board, data);
  OpenBoard back;
  return unpackBoard(data, sizeof(data), back) && sameGame(board.game, back.game) && back.flagMode == board.flagMode;
}

void testBoardRoundTrips() {
  std::mt19937 rng(4242);
  int moves = 0;
  int settled = 0;
  for (int g = 0; g < 500; g++) {
    OpenBoard board;
    start(board.game, static_cast<uint32_t>(rng()));
    board.flagMode = g % 2 == 1;
    check(roundTrips(board), "an untouched board doesn't round-trip");

    // The rest of the board plays on after a reload, so the restored copy is the
    // one that keeps going: whatever doesn't survive a save shows up as a
    // different game.
    for (int m = 0; m < 200 && !over(board.game); m++) {
      const int column = static_cast<int>(rng() % kColumns);
      const int row = static_cast<int>(rng() % kRows);
      const bool changed = rng() % 4 == 0 ? toggleFlag(board.game, column, row) : dig(board.game, column, row);
      if (!changed || over(board.game)) continue;
      moves++;
      uint8_t data[BOARD_BYTES];
      packBoard(board, data);
      OpenBoard back;
      check(unpackBoard(data, sizeof(data), back), "game " + std::to_string(g) + " move " + std::to_string(m) + " doesn't unpack");
      check(sameGame(board.game, back.game), "game " + std::to_string(g) + " move " + std::to_string(m) + " comes back different");
      board = back;
    }
    if (over(board.game)) settled++;
  }
  printf("%d moves packed and played on from the copy, %d games settled\n", moves, settled);
}

OpenBoard sampleBoard() {
  OpenBoard board;
  start(board.game, 99);
  dig(board.game, 4, 5);
  for (int column = 0; column < kColumns; column++) {
    for (int row = 0; row < kRows; row++) {
      if ((board.game.cell[column][row] & kMine) != 0) {
        toggleFlag(board.game, column, row);
        return board;
      }
    }
  }
  return board;
}

bool unpacks(const uint8_t* data, size_t size) {
  OpenBoard board;
  return unpackBoard(data, size, board);
}

int findCell(const Game& game, uint8_t has, uint8_t lacks) {
  for (int column = 0; column < kColumns; column++) {
    for (int row = 0; row < kRows; row++) {
      const uint8_t cell = game.cell[column][row];
      if ((cell & has) == has && (cell & lacks) == 0) return column * kRows + row;
    }
  }
  return -1;
}

void testBoardRejects() {
  const OpenBoard board = sampleBoard();
  check(board.game.status == Status::Playing, "the sample isn't being played");
  uint8_t good[BOARD_BYTES];
  packBoard(board, good);
  check(unpacks(good, sizeof(good)), "the sample doesn't unpack");
  check(!unpacks(good, sizeof(good) - 1), "a short board unpacks");

  uint8_t data[BOARD_BYTES];
  const auto damaged = [&](int at, uint8_t value) {
    memcpy(data, good, sizeof(data));
    data[at] = value;
    return unpacks(data, sizeof(data));
  };
  check(!damaged(84, 2), "a status of 2 unpacks");
  check(!damaged(85, 2), "a mode of 2 unpacks");
  check(!damaged(0, good[0] | 0x08), "a cell with a stray bit unpacks");

  const int mine = findCell(board.game, kMine, kFlagged);
  check(mine >= 0 && !damaged(mine, good[mine] & ~kMine), "nine mines unpack");
  check(mine >= 0 && !damaged(mine, good[mine] | kRevealed), "a dug mine unpacks");
  const int open = findCell(board.game, kRevealed, kMine);
  check(open >= 0 && !damaged(open, good[open] | kFlagged), "a flag on an open cell unpacks");
  const int covered = findCell(board.game, 0, kMine | kRevealed);
  check(covered >= 0 && !damaged(covered, good[covered] | kMine), "eleven mines unpack");

  // An untouched board holds nothing.
  OpenBoard fresh;
  start(fresh.game, 5);
  uint8_t empty[BOARD_BYTES];
  packBoard(fresh, empty);
  check(unpacks(empty, sizeof(empty)), "an untouched board doesn't unpack");
  empty[10] = kMine;
  check(!unpacks(empty, sizeof(empty)), "an untouched board with a mine unpacks");

  // Playing with nothing dug, or with every safe cell dug, can't be saved.
  memcpy(data, good, sizeof(data));
  for (int i = 0; i < kCells; i++) data[i] &= static_cast<uint8_t>(~kRevealed);
  check(!unpacks(data, sizeof(data)), "a board in play with nothing dug unpacks");
  memcpy(data, good, sizeof(data));
  for (int i = 0; i < kCells; i++) {
    if ((data[i] & kMine) == 0) data[i] = kRevealed;
  }
  check(!unpacks(data, sizeof(data)), "a won board unpacks as one in play");
}

void testHistory() {
  OpenBoard board = sampleBoard();
  while (!over(board.game)) {
    const int cell = findCell(board.game, 0, kRevealed | kFlagged);
    if (cell < 0) break;
    dig(board.game, cell / kRows, cell % kRows);
  }
  check(over(board.game), "the sample doesn't settle");

  History history;
  history.wins = 300;
  history.losses = 7;
  history.lastBoard = board.game;
  uint8_t data[HISTORY_BYTES];
  packHistory(history, data);
  History back;
  check(unpackHistory(data, sizeof(data), back), "a history doesn't unpack");
  check(back.wins == 300 && back.losses == 7, "the tally comes back wrong");
  check(memcmp(back.lastBoard.cell, board.game.cell, sizeof(board.game.cell)) == 0, "the last board comes back different");
  check(boardLost(back.lastBoard) == (board.game.status == Status::Lost), "boardLost disagrees with the rules");

  check(!unpackHistory(data, sizeof(data) - 1, back), "a short history unpacks");
  data[4] |= 0x10;
  check(!unpackHistory(data, sizeof(data), back), "a history cell with a stray bit unpacks");
  packHistory(history, data);
  for (int i = 4; i < static_cast<int>(HISTORY_BYTES); i++) data[i] &= static_cast<uint8_t>(~kMine);
  check(!unpackHistory(data, sizeof(data), back), "a history board with no mines unpacks");
}

}  // namespace

int main() {
  testBoardRoundTrips();
  testBoardRejects();
  testHistory();
  printf("%d checks, %d failed\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}

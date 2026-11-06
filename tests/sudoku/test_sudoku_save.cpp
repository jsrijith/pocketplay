// Sudoku's save packing. Puzzles at every level are played at random (taps, holds,
// undos, digit picks) and packed after every move: what comes back must be the
// same game, with the same answer and grade solved again from the clues, and undo
// must walk back exactly as the original would. Damaged saves must be refused.
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

#include "sudoku_game.h"
#include "sudoku_rules.h"
#include "sudoku_save.h"

using namespace sudoku;

namespace {

int checksRun = 0;
int checksFailed = 0;

void check(bool condition, const std::string& what) {
  checksRun++;
  if (condition) return;
  checksFailed++;
  if (checksFailed <= 20) printf("FAIL %s\n", what.c_str());
}

bool samePlay(const Game& a, const Game& b) {
  if (memcmp(a.puzzle.given, b.puzzle.given, sizeof(a.puzzle.given)) != 0) return false;
  if (memcmp(a.puzzle.solution, b.puzzle.solution, sizeof(a.puzzle.solution)) != 0) return false;
  if (a.puzzle.level != b.puzzle.level || a.puzzle.hardest != b.puzzle.hardest || a.puzzle.clues != b.puzzle.clues) return false;
  if (memcmp(a.entry, b.entry, sizeof(a.entry)) != 0 || memcmp(a.note, b.note, sizeof(a.note)) != 0) return false;
  if (a.undoCount != b.undoCount || a.undoNext != b.undoNext) return false;
  for (int i = 0; i < a.undoCount; i++) {
    const int slot = (a.undoNext + kUndoDepth - 1 - i) % kUndoDepth;
    const Change& x = a.undo[slot];
    const Change& y = b.undo[slot];
    if (x.cell != y.cell || x.entry != y.entry || x.note != y.note) return false;
  }
  return a.armed == b.armed && a.hintCell == b.hintCell && a.hintDigit == b.hintDigit && a.hintTechnique == b.hintTechnique &&
         a.hintsUsed == b.hintsUsed && a.solvedFlag == b.solvedFlag && a.elapsedMs == b.elapsedMs;
}

bool roundTrip(const Saved& saved, Saved& back) {
  uint8_t data[SAVE_BYTES];
  packGame(saved, data);
  return unpackGame(data, sizeof(data), back);
}

void randomMove(Game& game, std::mt19937& rng) {
  const int cell = static_cast<int>(rng() % kCells);
  switch (rng() % 6) {
    case 0:
      holdCell(game, cell);
      break;
    case 1:
      undoOnce(game);
      break;
    case 2:
      game.armed = static_cast<uint8_t>(1 + rng() % kSize);
      break;
    case 3: {
      // The right digit, so some games get solved.
      if (!isGiven(game, cell)) {
        game.armed = game.puzzle.solution[cell];
        tapCell(game, cell);
      }
      break;
    }
    default:
      tapCell(game, cell);
      break;
  }
}

void testRoundTrips() {
  std::mt19937 rng(777);
  uint32_t seed = 12345;
  Workspace work{};
  int moves = 0;
  int solved = 0;
  for (int g = 0; g < 40; g++) {
    const Level level = static_cast<Level>(g % kLevelCount);
    Puzzle puzzle;
    while (!generate(puzzle, level, work, seed, 24)) {
    }
    Saved saved;
    startGame(saved.game, puzzle);
    saved.hasGame = true;
    saved.menuLevel = level;
    Saved back;
    check(roundTrip(saved, back) && samePlay(saved.game, back.game), "a new puzzle at level " + std::to_string(g % 4) + " doesn't round-trip");
    check(back.game.puzzle.level == level, "a puzzle's level isn't graded back the same");

    for (int m = 0; m < 400 && saved.game.solvedFlag == 0; m++) {
      randomMove(saved.game, rng);
      saved.game.elapsedMs += 1000;
      moves++;
      const bool ok = roundTrip(saved, back);
      check(ok, "game " + std::to_string(g) + " move " + std::to_string(m) + " doesn't unpack");
      if (!ok) break;
      check(samePlay(saved.game, back.game), "game " + std::to_string(g) + " move " + std::to_string(m) + " comes back different");
      // Play on from the copy, so whatever a save loses shows up later.
      saved.game = back.game;
    }
    // Every other game is finished by hand, so a solved save is tested too.
    if (g % 2 == 0) {
      for (int cell = 0; cell < kCells; cell++) {
        if (isGiven(saved.game, cell) || saved.game.entry[cell] == saved.game.puzzle.solution[cell]) continue;
        saved.game.armed = saved.game.puzzle.solution[cell];
        tapCell(saved.game, cell);
      }
    }
    if (saved.game.solvedFlag != 0) {
      solved++;
      check(roundTrip(saved, back) && back.game.solvedFlag == 1, "a solved game doesn't come back solved");
    }

    // Undo the restored copy and the original together, all the way.
    roundTrip(saved, back);
    Game original = saved.game;
    while (canUndo(original)) {
      check(undoOnce(back.game), "the copy runs out of undo early");
      undoOnce(original);
      check(memcmp(original.entry, back.game.entry, sizeof(original.entry)) == 0 && memcmp(original.note, back.game.note, sizeof(original.note)) == 0,
            "undo after a reload goes somewhere else");
    }
    check(!canUndo(back.game), "the copy has undo left over");
  }
  printf("%d moves packed and played on from the copy, %d of 40 games solved\n", moves, solved);
  check(solved > 0, "no game was solved, so a solved save wasn't tested");

  Saved fresh;
  Saved back;
  check(roundTrip(fresh, back) && !back.hasGame, "nothing started doesn't round-trip");
}

Saved sample() {
  uint32_t seed = 99;
  Workspace work{};
  Puzzle puzzle;
  while (!generate(puzzle, Level::Medium, work, seed, 24)) {
  }
  Saved saved;
  startGame(saved.game, puzzle);
  saved.hasGame = true;
  saved.menuLevel = Level::Medium;
  int placed = 0;
  for (int cell = 0; cell < kCells && placed < 3; cell++) {
    if (isGiven(saved.game, cell)) continue;
    saved.game.armed = puzzle.solution[cell];
    tapCell(saved.game, cell);
    placed++;
  }
  for (int cell = kCells - 1; cell >= 0; cell--) {
    if (!isGiven(saved.game, cell) && saved.game.entry[cell] == 0) {
      holdCell(saved.game, cell);
      break;
    }
  }
  return saved;
}

void testRejects() {
  const Saved saved = sample();
  uint8_t good[SAVE_BYTES];
  packGame(saved, good);
  Saved scratch;
  check(unpackGame(good, sizeof(good), scratch), "the sample doesn't unpack");
  check(!unpackGame(good, sizeof(good) - 1, scratch), "a short save unpacks");

  uint8_t data[SAVE_BYTES];
  const auto damaged = [&](size_t at, uint8_t value) {
    memcpy(data, good, sizeof(data));
    data[at] = value;
    return unpackGame(data, sizeof(data), scratch);
  };
  check(!damaged(0, kLevelCount), "a fifth level unpacks");
  check(!damaged(1, 2), "a started flag of 2 unpacks");
  check(!damaged(2, 0) && !damaged(2, 10), "an armed digit out of range unpacks");
  check(!damaged(3, kCells + 1), "a hint cell past the grid unpacks");
  check(!damaged(4, 10), "a hint digit of 10 unpacks");
  check(!damaged(5, kTechniqueCount), "an unknown technique unpacks");
  check(!damaged(7, 1), "an unsolved board marked solved unpacks");
  check(!damaged(12, kUndoDepth + 1), "more undo than the ring holds unpacks");
  check(!damaged(13, kUndoDepth), "an undo position past the ring unpacks");

  int clue = -1;
  int empty = -1;
  int filled = -1;
  for (int cell = 0; cell < kCells; cell++) {
    if (isGiven(saved.game, cell)) {
      if (clue < 0) clue = cell;
    } else if (saved.game.entry[cell] != 0) {
      if (filled < 0) filled = cell;
    } else if (saved.game.note[cell] == 0 && empty < 0) {
      empty = cell;
    }
  }
  check(!damaged(14 + clue, 0x1A), "a clue of 10 unpacks");
  check(!damaged(14 + clue, 0x31), "a cell with a stray bit unpacks");
  check(!damaged(14 + empty, 10), "a digit of 10 unpacks");
  // Taking a clue away leaves a puzzle with more than one answer, or one the
  // solver can't finish: either way it isn't this puzzle any more.
  int ambiguous = 0;
  for (int cell = 0; cell < kCells; cell++) {
    if (isGiven(saved.game, cell) && !damaged(14 + cell, 0)) ambiguous++;
  }
  check(ambiguous < saved.game.puzzle.clues, "removing any clue still unpacks");
  memcpy(data, good, sizeof(data));
  data[95 + filled * 2 + 1] = 1;
  check(!unpackGame(data, sizeof(data), scratch), "a mark on a filled cell unpacks");
  memcpy(data, good, sizeof(data));
  data[95 + empty * 2] = 0x02;
  check(!unpackGame(data, sizeof(data), scratch), "a mark past digit 9 unpacks");

  // The newest undo step, pointed at a clue.
  memcpy(data, good, sizeof(data));
  const int newest = (saved.game.undoNext + kUndoDepth - 1) % kUndoDepth;
  data[257 + newest * 4] = static_cast<uint8_t>(clue);
  check(!unpackGame(data, sizeof(data), scratch), "an undo step on a clue unpacks");
  data[257 + newest * 4] = kCells;
  check(!unpackGame(data, sizeof(data), scratch), "an undo step past the grid unpacks");

  // Nothing started, with something on the board.
  Saved none;
  uint8_t blank[SAVE_BYTES];
  packGame(none, blank);
  blank[14 + 5] = 3;
  check(!unpackGame(blank, sizeof(blank), scratch), "nothing started, with a digit, unpacks");
}

void testRecord() {
  Record record;
  recordSolve(record, Level::Hard, 300000, 0);
  recordSolve(record, Level::Easy, 90000, 2);
  record.hintsTaken = 7;
  uint8_t data[RECORD_BYTES];
  packRecord(record, data);
  Record back;
  check(unpackRecord(data, sizeof(data), back), "a record doesn't unpack");
  check(memcmp(back.solved, record.solved, sizeof(record.solved)) == 0 && memcmp(back.bestMs, record.bestMs, sizeof(record.bestMs)) == 0 &&
            back.hintsTaken == 7,
        "a record comes back different");
  check(!unpackRecord(data, sizeof(data) - 1, back), "a short record unpacks");
  data[0] = 0;
  data[1] = 0;
  data[11] = 1;  // a best time at EASY with no EASY solves
  check(!unpackRecord(data, sizeof(data), back), "a best time with no solve unpacks");
}

}  // namespace

int main() {
  testRoundTrips();
  testRejects();
  testRecord();
  printf("%d checks, %d failed\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}

#include "sudoku_save.h"

namespace sudoku {

namespace {

constexpr uint8_t CLUE = 0x10;
constexpr size_t CELLS_AT = 14;
constexpr size_t NOTES_AT = CELLS_AT + kCells;
constexpr size_t UNDO_AT = NOTES_AT + kCells * 2;
static_assert(UNDO_AT + kUndoDepth * 4 == SAVE_BYTES, "the layout adds up");

void put16(uint8_t* out, uint16_t value) {
  out[0] = static_cast<uint8_t>(value >> 8);
  out[1] = static_cast<uint8_t>(value);
}
void put32(uint8_t* out, uint32_t value) {
  out[0] = static_cast<uint8_t>(value >> 24);
  out[1] = static_cast<uint8_t>(value >> 16);
  out[2] = static_cast<uint8_t>(value >> 8);
  out[3] = static_cast<uint8_t>(value);
}
uint16_t get16(const uint8_t* in) { return static_cast<uint16_t>(in[0] << 8 | in[1]); }
uint32_t get32(const uint8_t* in) {
  return static_cast<uint32_t>(in[0]) << 24 | static_cast<uint32_t>(in[1]) << 16 | static_cast<uint32_t>(in[2]) << 8 | in[3];
}

}  // namespace

void packGame(const Saved& saved, uint8_t* out) {
  const Game& game = saved.game;
  out[0] = static_cast<uint8_t>(saved.menuLevel);
  out[1] = saved.hasGame ? 1 : 0;
  out[2] = game.armed;
  out[3] = game.hintCell;
  out[4] = game.hintDigit;
  out[5] = game.hintTechnique;
  out[6] = game.hintsUsed;
  out[7] = game.solvedFlag;
  put32(out + 8, game.elapsedMs);
  out[12] = game.undoCount;
  out[13] = game.undoNext;
  for (int cell = 0; cell < kCells; cell++) {
    out[CELLS_AT + cell] = game.puzzle.given[cell] != 0 ? static_cast<uint8_t>(CLUE | game.puzzle.given[cell]) : game.entry[cell];
    put16(out + NOTES_AT + cell * 2, game.note[cell]);
  }
  for (int slot = 0; slot < kUndoDepth; slot++) {
    const Change& change = game.undo[slot];
    uint8_t* at = out + UNDO_AT + slot * 4;
    at[0] = change.cell;
    at[1] = change.entry;
    put16(at + 2, change.note);
  }
}

bool unpackGame(const uint8_t* data, size_t size, Saved& saved) {
  if (size != SAVE_BYTES) return false;
  if (data[0] >= kLevelCount || data[1] > 1 || data[7] > 1) return false;
  if (data[2] < 1 || data[2] > kSize || data[3] > kNoCell || data[4] > kSize || data[5] >= kTechniqueCount) return false;
  if (data[12] > kUndoDepth || data[13] >= kUndoDepth) return false;

  Saved read;
  read.menuLevel = static_cast<Level>(data[0]);
  read.hasGame = data[1] == 1;
  Game& game = read.game;
  game.armed = data[2];
  game.hintCell = data[3];
  game.hintDigit = data[4];
  game.hintTechnique = data[5];
  game.hintsUsed = data[6];
  game.solvedFlag = data[7];
  game.elapsedMs = get32(data + 8);
  game.undoCount = data[12];
  game.undoNext = data[13];

  for (int cell = 0; cell < kCells; cell++) {
    const uint8_t value = data[CELLS_AT + cell];
    if (value & CLUE) {
      const uint8_t digit = value & 0x0F;
      if ((value & 0xE0) != 0 || digit < 1 || digit > kSize) return false;
      game.puzzle.given[cell] = digit;
    } else {
      if (value > kSize) return false;
      game.entry[cell] = value;
    }
    game.note[cell] = get16(data + NOTES_AT + cell * 2);
    if (game.note[cell] > kAllDigits) return false;
    // Writing a digit clears the cell's marks, and pencilling clears its digit.
    if (valueAt(game, cell) != 0 && game.note[cell] != 0) return false;
  }
  for (int slot = 0; slot < kUndoDepth; slot++) {
    const uint8_t* at = data + UNDO_AT + slot * 4;
    game.undo[slot] = Change{at[0], at[1], get16(at + 2)};
  }
  // Only the live steps are checked: the others are whatever the ring held before.
  for (int i = 0; i < game.undoCount; i++) {
    const Change& change = game.undo[(game.undoNext + kUndoDepth - 1 - i) % kUndoDepth];
    if (change.cell >= kCells || game.puzzle.given[change.cell] != 0) return false;
    if (change.entry > kSize || change.note > kAllDigits) return false;
  }

  if (!read.hasGame) {
    // Nothing started: an empty board and nothing to undo or hint.
    for (int cell = 0; cell < kCells; cell++) {
      if (game.puzzle.given[cell] != 0 || game.entry[cell] != 0 || game.note[cell] != 0) return false;
    }
    if (game.undoCount != 0 || game.solvedFlag != 0) return false;
    saved = read;
    return true;
  }

  // The answer and the grade come from the clues, as CrossPlay's load does.
  Grid grid;
  if (!load(grid, game.puzzle.given)) return false;
  const SolveReport report = solve(grid, ceilingFor(Level::Expert));
  if (!report.solved || report.broken) return false;
  int clues = 0;
  for (int cell = 0; cell < kCells; cell++) {
    game.puzzle.solution[cell] = grid.value[cell];
    if (game.puzzle.given[cell] != 0) ++clues;
  }
  game.puzzle.hardest = report.hardest;
  game.puzzle.level = levelOf(report.hardest);
  game.puzzle.clues = static_cast<uint8_t>(clues);
  // Solved means the board is right. The other way round needn't hold: undo clears
  // the flag, even when it undoes a mark on a grid that is still complete.
  if (game.solvedFlag == 1 && !isSolved(game)) return false;

  saved = read;
  return true;
}

void packRecord(const Record& record, uint8_t* out) {
  for (int i = 0; i < kLevelCount; i++) {
    put16(out + i * 2, record.solved[i]);
    put32(out + 8 + i * 4, record.bestMs[i]);
  }
  put16(out + 24, record.hintsTaken);
}

bool unpackRecord(const uint8_t* data, size_t size, Record& record) {
  if (size != RECORD_BYTES) return false;
  Record read;
  for (int i = 0; i < kLevelCount; i++) {
    read.solved[i] = get16(data + i * 2);
    read.bestMs[i] = get32(data + 8 + i * 4);
    // A best time comes with a solve.
    if (read.bestMs[i] != 0 && read.solved[i] == 0) return false;
  }
  read.hintsTaken = get16(data + 24);
  record = read;
  return true;
}

}  // namespace sudoku

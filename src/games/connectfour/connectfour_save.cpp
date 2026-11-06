#include "connectfour_save.h"

namespace connectfour {

namespace {

constexpr uint8_t NONE = 0xFF;

void putCount(uint8_t* out, int value) {
  out[0] = static_cast<uint8_t>(value >> 8);
  out[1] = static_cast<uint8_t>(value);
}

int getCount(const uint8_t* in) { return in[0] << 8 | in[1]; }

// Refuses a cell other than empty, light or dark, and a disc above a gap.
bool unpackCells(const uint8_t* in, uint8_t cells[kColumns][kRows]) {
  for (int column = 0; column < kColumns; ++column) {
    bool sawEmpty = false;
    for (int row = 0; row < kRows; ++row) {
      const uint8_t cell = in[column * kRows + row];
      if (cell > kDark) return false;
      if (cell == kEmpty) {
        sawEmpty = true;
      } else if (sawEmpty) {
        return false;
      }
      cells[column][row] = cell;
    }
  }
  return true;
}

// Four cells in a straight line, each a step of one in the same direction.
bool straight(const int line[kLine]) {
  for (int i = 0; i < kLine; ++i) {
    if (line[i] < 0 || line[i] >= kCells) return false;
  }
  const int dc = line[1] / kRows - line[0] / kRows;
  const int dr = line[1] % kRows - line[0] % kRows;
  if (dc < -1 || dc > 1 || dr < -1 || dr > 1 || (dc == 0 && dr == 0)) return false;
  for (int i = 1; i < kLine; ++i) {
    if (line[i] / kRows - line[i - 1] / kRows != dc || line[i] % kRows - line[i - 1] % kRows != dr) return false;
  }
  return true;
}

}  // namespace

void packGame(const Game& game, uint8_t* out) {
  for (int column = 0; column < kColumns; ++column) {
    for (int row = 0; row < kRows; ++row) out[column * kRows + row] = game.cell[column][row];
  }
  out[42] = game.lastColumn == kNoColumn ? NONE : static_cast<uint8_t>(game.lastColumn);
  out[43] = game.lastRow == kNoColumn ? NONE : static_cast<uint8_t>(game.lastRow);
  out[44] = game.turn;
}

bool unpackGame(const uint8_t* data, size_t size, Game& game) {
  if (size != GAME_BYTES) return false;
  Game read{};
  if (!unpackCells(data, read.cell)) return false;
  if (data[42] != NONE && data[42] >= kColumns) return false;
  if (data[43] != NONE && data[43] >= kRows) return false;
  read.lastColumn = data[42] == NONE ? kNoColumn : static_cast<int8_t>(data[42]);
  read.lastRow = data[43] == NONE ? kNoColumn : static_cast<int8_t>(data[43]);
  read.turn = data[44];
  // A settled game is never saved, so whatever is here is still running.
  read.outcome = Outcome::Running;
  if (!plausible(read)) return false;
  if (read.lastColumn != kNoColumn) {
    // The last disc sits on top of its column, and belongs to whoever just moved.
    const int top = read.lastRow + 1;
    if (top < kRows && read.cell[read.lastColumn][top] != kEmpty) return false;
    if (read.cell[read.lastColumn][read.lastRow] != other(read.turn)) return false;
  }
  game = read;
  return true;
}

void packRecord(const Record& record, uint8_t* out) {
  putCount(out, record.wins);
  putCount(out + 2, record.losses);
  putCount(out + 4, record.draws);
  out[6] = record.hasLast ? 1 : 0;
  out[7] = record.lastOutcome;
  for (int i = 0; i < kLine; ++i) out[8 + i] = record.lastLine[i] < 0 ? NONE : static_cast<uint8_t>(record.lastLine[i]);
  for (int i = 0; i < kCells; ++i) out[12 + i] = record.lastCells[i];
}

bool unpackRecord(const uint8_t* data, size_t size, Record& record) {
  if (size != RECORD_BYTES || data[6] > 1 || data[7] > LAST_DRAW) return false;
  Record read;
  read.wins = getCount(data);
  read.losses = getCount(data + 2);
  read.draws = getCount(data + 4);
  read.hasLast = data[6] == 1;
  read.lastOutcome = data[7];
  for (int i = 0; i < kLine; ++i) read.lastLine[i] = data[8 + i] == NONE ? -1 : data[8 + i];
  Game board{};
  if (!unpackCells(data + 12, board.cell)) return false;
  for (int column = 0; column < kColumns; ++column) {
    for (int row = 0; row < kRows; ++row) read.lastCells[column * kRows + row] = board.cell[column][row];
  }

  bool noLine = true;
  for (int i = 0; i < kLine; ++i) noLine = noLine && read.lastLine[i] < 0;
  if (!read.hasLast) {
    if (read.lastOutcome != LAST_DRAW || !noLine || discCount(board) != 0) return false;
  } else if (read.lastOutcome == LAST_DRAW) {
    if (!noLine || !boardFull(board) || winningLine(board, kLight, nullptr) || winningLine(board, kDark, nullptr)) return false;
    if (read.draws == 0) return false;
  } else {
    // The record is drawn from your side, so a win is a line of light discs.
    const uint8_t side = read.lastOutcome == LAST_WON ? kLight : kDark;
    if (!straight(read.lastLine)) return false;
    for (int i = 0; i < kLine; ++i) {
      if (read.lastCells[read.lastLine[i]] != side) return false;
    }
    if ((read.lastOutcome == LAST_WON ? read.wins : read.losses) == 0) return false;
  }
  record = read;
  return true;
}

}  // namespace connectfour

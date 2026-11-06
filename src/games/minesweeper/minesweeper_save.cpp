#include "minesweeper_save.h"

namespace minesweeper {

namespace {

constexpr uint8_t CELL_BITS = kMine | kRevealed | kFlagged;

void packCells(const Game& game, uint8_t* out) {
  for (int column = 0; column < kColumns; ++column) {
    for (int row = 0; row < kRows; ++row) out[column * kRows + row] = game.cell[column][row];
  }
}

// Reads the cells into game, refusing a stray bit, and returns the mine count.
bool unpackCells(const uint8_t* in, Game& game, int& mines) {
  mines = 0;
  for (int column = 0; column < kColumns; ++column) {
    for (int row = 0; row < kRows; ++row) {
      const uint8_t cell = in[column * kRows + row];
      if ((cell & ~CELL_BITS) != 0) return false;
      if (cell & kMine) ++mines;
      game.cell[column][row] = cell;
    }
  }
  return true;
}

}  // namespace

void packBoard(const OpenBoard& board, uint8_t* out) {
  packCells(board.game, out);
  out[80] = static_cast<uint8_t>(board.game.rng >> 24);
  out[81] = static_cast<uint8_t>(board.game.rng >> 16);
  out[82] = static_cast<uint8_t>(board.game.rng >> 8);
  out[83] = static_cast<uint8_t>(board.game.rng);
  out[84] = board.game.status == Status::Playing ? 1 : 0;
  out[85] = board.flagMode ? 1 : 0;
}

bool unpackBoard(const uint8_t* data, size_t size, OpenBoard& board) {
  if (size != BOARD_BYTES || data[84] > 1 || data[85] > 1) return false;

  OpenBoard read;
  int mines = 0;
  if (!unpackCells(data, read.game, mines)) return false;
  read.game.rng = static_cast<uint32_t>(data[80]) << 24 | static_cast<uint32_t>(data[81]) << 16 | static_cast<uint32_t>(data[82]) << 8 | data[83];
  read.game.status = data[84] == 1 ? Status::Playing : Status::Fresh;
  read.flagMode = data[85] == 1;

  int revealed = 0;
  for (int column = 0; column < kColumns; ++column) {
    for (int row = 0; row < kRows; ++row) {
      const uint8_t cell = read.game.cell[column][row];
      if (read.game.status == Status::Fresh && (cell & (kMine | kRevealed)) != 0) return false;
      if ((cell & kRevealed) && (cell & (kMine | kFlagged))) return false;
      if (cell & kRevealed) ++revealed;
    }
  }
  if (read.game.status == Status::Playing) {
    // The first dig lays the mines and opens at least one cell, and opening every
    // safe cell is a win, which is never saved.
    if (mines != kMines || revealed == 0 || allSafeCellsRevealed(read.game)) return false;
  }

  board = read;
  return true;
}

void packHistory(const History& history, uint8_t* out) {
  out[0] = static_cast<uint8_t>(history.wins >> 8);
  out[1] = static_cast<uint8_t>(history.wins);
  out[2] = static_cast<uint8_t>(history.losses >> 8);
  out[3] = static_cast<uint8_t>(history.losses);
  packCells(history.lastBoard, out + 4);
}

bool unpackHistory(const uint8_t* data, size_t size, History& history) {
  if (size != HISTORY_BYTES) return false;
  History read;
  read.wins = data[0] << 8 | data[1];
  read.losses = data[2] << 8 | data[3];
  int mines = 0;
  if (!unpackCells(data + 4, read.lastBoard, mines) || mines != kMines) return false;
  history = read;
  return true;
}

bool boardLost(const Game& board) {
  for (int column = 0; column < kColumns; ++column) {
    for (int row = 0; row < kRows; ++row) {
      const uint8_t cell = board.cell[column][row];
      if ((cell & kMine) != 0 && (cell & kRevealed) != 0) return true;
    }
  }
  return false;
}

}  // namespace minesweeper

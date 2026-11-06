#include "knucklebones_save.h"

namespace knucklebones {

namespace {

void packGrid(const Grid& grid, uint8_t* out) {
  for (int column = 0; column < kColumns; ++column) {
    for (int row = 0; row < kRows; ++row) out[column * kRows + row] = grid.cell[column][row];
  }
}

// Refuses a face past 6 and a die above a gap: dice always stack from the near end.
bool unpackGrid(const uint8_t* in, Grid& grid) {
  for (int column = 0; column < kColumns; ++column) {
    bool seenEmpty = false;
    for (int row = 0; row < kRows; ++row) {
      const uint8_t value = in[column * kRows + row];
      if (value > kFaces) return false;
      if (value == kEmpty) {
        seenEmpty = true;
      } else if (seenEmpty) {
        return false;
      }
      grid.cell[column][row] = value;
    }
  }
  return true;
}

bool empty(const Grid& grid) {
  for (int column = 0; column < kColumns; ++column) {
    if (columnCount(grid, column) != 0) return false;
  }
  return true;
}

void putCount(uint8_t* out, int value) {
  out[0] = static_cast<uint8_t>(value >> 8);
  out[1] = static_cast<uint8_t>(value);
}

int getCount(const uint8_t* in) { return in[0] << 8 | in[1]; }

}  // namespace

void packMatch(const Game& game, uint8_t* out) {
  packGrid(game.grid[0], out);
  packGrid(game.grid[1], out + 9);
  out[18] = game.turn;
  out[19] = game.die;
  out[20] = static_cast<uint8_t>(game.rng >> 24);
  out[21] = static_cast<uint8_t>(game.rng >> 16);
  out[22] = static_cast<uint8_t>(game.rng >> 8);
  out[23] = static_cast<uint8_t>(game.rng);
}

bool unpackMatch(const uint8_t* data, size_t size, Game& game) {
  if (size != MATCH_BYTES) return false;
  Game read{};
  if (!unpackGrid(data, read.grid[0]) || !unpackGrid(data + 9, read.grid[1])) return false;
  read.turn = data[18];
  read.die = data[19];
  read.rng = static_cast<uint32_t>(data[20]) << 24 | static_cast<uint32_t>(data[21]) << 16 | static_cast<uint32_t>(data[22]) << 8 | data[23];
  // xorshift never reaches zero from anything else, so a zero state was never saved.
  if (read.turn >= kSeats || read.die < 1 || read.die > kFaces || read.rng == 0) return false;
  if (!plausible(read) || over(read)) return false;
  game = read;
  return true;
}

void packRecord(const Record& record, uint8_t* out) {
  putCount(out, record.wins);
  putCount(out + 2, record.losses);
  putCount(out + 4, record.draws);
  out[6] = record.hasLast ? 1 : 0;
  packGrid(record.lastYours, out + 7);
  packGrid(record.lastTheirs, out + 16);
}

bool unpackRecord(const uint8_t* data, size_t size, Record& record) {
  if (size != RECORD_BYTES || data[6] > 1) return false;
  Record read;
  read.wins = getCount(data);
  read.losses = getCount(data + 2);
  read.draws = getCount(data + 4);
  read.hasLast = data[6] == 1;
  if (!unpackGrid(data + 7, read.lastYours) || !unpackGrid(data + 16, read.lastTheirs)) return false;
  if (read.hasLast) {
    // A match ends the moment either grid fills, so one of them always has.
    if (!full(read.lastYours) && !full(read.lastTheirs)) return false;
    if (read.wins + read.losses + read.draws == 0) return false;
  } else if (!empty(read.lastYours) || !empty(read.lastTheirs)) {
    return false;
  }
  record = read;
  return true;
}

}  // namespace knucklebones

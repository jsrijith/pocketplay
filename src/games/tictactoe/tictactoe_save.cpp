#include "tictactoe_save.h"

namespace tictactoe {

size_t packGame(const Saved& saved, uint8_t* out) {
  for (int i = 0; i < kCells; i++) {
    out[i] = saved.game.cell[i];
  }
  out[kCells] = saved.game.turn;
  out[kCells + 1] = static_cast<uint8_t>(saved.mode);
  out[kCells + 2] = saved.computer;
  return SAVE_BYTES;
}

namespace {

// The board and turn, which both versions start with.
bool unpackBoard(const uint8_t* in, Game& game) {
  Game loaded;
  int xCount = 0;
  int oCount = 0;
  for (int i = 0; i < kCells; i++) {
    const uint8_t cell = in[i];
    if (cell == kX) {
      xCount++;
    } else if (cell == kO) {
      oCount++;
    } else if (cell != kEmpty) {
      return false;
    }
    loaded.cell[i] = cell;
  }

  // X opens and players alternate, so the counts decide whose turn it must be.
  const uint8_t turn = in[kCells];
  const bool xToMove = xCount == oCount && turn == kX;
  const bool oToMove = xCount == oCount + 1 && turn == kO;
  if (!xToMove && !oToMove) return false;

  loaded.turn = turn;
  loaded.outcome = Outcome::Running;
  if (winningLine(loaded, kX, nullptr) || winningLine(loaded, kO, nullptr) || boardFull(loaded)) return false;

  game = loaded;
  return true;
}

}  // namespace

bool unpackGame(uint8_t version, const uint8_t* in, size_t size, Saved& saved) {
  Saved loaded;
  if (version == 1 && size == SAVE_BYTES_V1) {
    if (!unpackBoard(in, loaded.game)) return false;
    loaded.mode = Mode::TwoPlayers;
    loaded.computer = kEmpty;
    saved = loaded;
    return true;
  }
  if (version != SAVE_VERSION || size != SAVE_BYTES) return false;
  if (!unpackBoard(in, loaded.game)) return false;

  const uint8_t mode = in[kCells + 1];
  const uint8_t computer = in[kCells + 2];
  if (mode >= kModeCount) return false;
  loaded.mode = static_cast<Mode>(mode);
  if (loaded.mode == Mode::TwoPlayers ? computer != kEmpty : computer != kX && computer != kO) return false;
  loaded.computer = computer;

  saved = loaded;
  return true;
}

}  // namespace tictactoe

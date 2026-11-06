#include "solitaire_save.h"

namespace solitaire {

namespace {

constexpr uint8_t TURNED = 0x20;
constexpr uint8_t STOCK_DEAL = 0x40;
constexpr uint8_t COUNT_MASK = 0x1F;
constexpr uint8_t SPARE_BIT = 0x40;  // in a card byte: never set by cards.h

bool isTableauPile(int pile) { return pile >= kFirstTableau; }

// Which way up a card must be in its pile: the stock is all face down, the waste
// and foundations all face up, and a column is face down then face up, with its
// top card face up.
bool facesFit(int pile, const uint8_t* cards, int count) {
  if (pile == kStockPile) {
    for (int c = 0; c < count; c++) {
      if (isFaceUp(cards[c])) return false;
    }
    return true;
  }
  if (!isTableauPile(pile)) {
    for (int c = 0; c < count; c++) {
      if (!isFaceUp(cards[c])) return false;
    }
    return true;
  }
  if (count > 0 && !isFaceUp(cards[count - 1])) return false;
  bool seenUp = false;
  for (int c = 0; c < count; c++) {
    if (isFaceUp(cards[c])) {
      seenUp = true;
    } else if (seenUp) {
      return false;
    }
  }
  return true;
}

}  // namespace

size_t packGame(const Game& game, uint8_t* out) {
  Game::Save state;
  game.save(state);

  size_t at = 0;
  out[at++] = static_cast<uint8_t>(state.seed >> 24);
  out[at++] = static_cast<uint8_t>(state.seed >> 16);
  out[at++] = static_cast<uint8_t>(state.seed >> 8);
  out[at++] = static_cast<uint8_t>(state.seed);
  out[at++] = state.drawThree ? 1 : 0;
  out[at++] = static_cast<uint8_t>(state.moves >> 8);
  out[at++] = static_cast<uint8_t>(state.moves);
  for (int p = 0; p < kPileCount; p++) out[at++] = state.counts[p];
  for (int p = 0; p < kPileCount; p++) {
    for (int c = 0; c < state.counts[p]; c++) out[at++] = state.piles[p][c];
  }

  const int steps = game.undoDepth();
  out[at++] = static_cast<uint8_t>(steps);
  for (int i = 0; i < steps; i++) {
    const Move& step = game.undoStep(i);
    out[at++] = static_cast<uint8_t>(step.from << 4 | step.to);
    out[at++] = static_cast<uint8_t>(step.count | (step.flipped ? TURNED : 0) | (step.stock ? STOCK_DEAL : 0));
  }
  return at;
}

bool unpackGame(const uint8_t* data, size_t size, Game& game) {
  if (size < SAVE_HEADER_BYTES) return false;
  const int steps = data[SAVE_HEADER_BYTES - 1];
  if (steps > kUndoDepth || size != SAVE_HEADER_BYTES + steps * UNDO_STEP_BYTES) return false;

  Game::Save state;
  state.seed = static_cast<uint32_t>(data[0]) << 24 | static_cast<uint32_t>(data[1]) << 16 | static_cast<uint32_t>(data[2]) << 8 | data[3];
  if (data[4] > 1) return false;
  state.drawThree = data[4] == 1;
  state.moves = static_cast<uint16_t>(data[5] << 8 | data[6]);

  int total = 0;
  for (int p = 0; p < kPileCount; p++) {
    state.counts[p] = data[7 + p];
    if (state.counts[p] > kPileCapacity) return false;
    total += state.counts[p];
  }
  if (total != kDeck) return false;

  size_t at = 7 + kPileCount;
  for (int p = 0; p < kPileCount; p++) {
    for (int c = 0; c < state.counts[p]; c++) {
      const uint8_t card = data[at++];
      if ((card & SPARE_BIT) != 0 || rankOf(card) >= kRanks) return false;
      state.piles[p][c] = card;
    }
    if (!facesFit(p, state.piles[p], state.counts[p])) return false;
  }

  // restore() refuses a deck with a card missing or repeated.
  Game trial;
  if (!trial.restore(state)) return false;

  Move history[kUndoDepth];
  at = SAVE_HEADER_BYTES;
  for (int i = 0; i < steps; i++) {
    const uint8_t piles = data[at++];
    const uint8_t rest = data[at++];
    if ((rest & 0x80) != 0) return false;
    history[i].from = static_cast<uint8_t>(piles >> 4);
    history[i].to = static_cast<uint8_t>(piles & 0x0F);
    history[i].count = rest & COUNT_MASK;
    history[i].flipped = (rest & TURNED) != 0;
    history[i].stock = (rest & STOCK_DEAL) != 0;
  }
  if (!trial.restoreUndo(history, steps)) return false;

  game = trial;
  return true;
}

}  // namespace solitaire

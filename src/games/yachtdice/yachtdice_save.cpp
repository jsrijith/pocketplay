#include "yachtdice_save.h"

namespace yachtdice {
namespace {

namespace yz = yahtzee;

constexpr uint8_t EMPTY_BOX = 0xFF;

void packCard(const yz::Card& card, uint8_t* out) {
  for (int i = 0; i < yz::kCategories; ++i) {
    out[i] = card.box[i] == yz::kUnscored ? EMPTY_BOX : static_cast<uint8_t>(card.box[i]);
  }
  out[yz::kCategories] = card.yahtzeeBonuses;
}

// Scores reach 50 at most, so any byte above 127 but the empty mark is refused here,
// and plausible() checks the rest.
bool unpackCard(const uint8_t* in, yz::Card& card) {
  for (int i = 0; i < yz::kCategories; ++i) {
    if (in[i] == EMPTY_BOX) {
      card.box[i] = yz::kUnscored;
    } else if (in[i] <= 127) {
      card.box[i] = static_cast<int8_t>(in[i]);
    } else {
      return false;
    }
  }
  card.yahtzeeBonuses = in[yz::kCategories];
  return true;
}

void put16(uint8_t* out, int value) {
  out[0] = static_cast<uint8_t>(value);
  out[1] = static_cast<uint8_t>(value >> 8);
}

int get16(const uint8_t* in) { return in[0] | (in[1] << 8); }

constexpr int CARD_BYTES = yz::kCategories + 1;
constexpr int DICE_AT = 2 * CARD_BYTES;
constexpr int HELD_AT = DICE_AT + yz::kDice;
constexpr int ROLLS_AT = HELD_AT + 1;
constexpr int TURN_AT = ROLLS_AT + 1;
constexpr int RANDOM_AT = TURN_AT + 1;
static_assert(RANDOM_AT + 4 == static_cast<int>(GAME_BYTES), "the game save's layout and size disagree");

}  // namespace

void packGame(const yz::Game& game, uint8_t* out) {
  packCard(game.card[0], out);
  packCard(game.card[1], out + CARD_BYTES);
  for (int i = 0; i < yz::kDice; ++i) out[DICE_AT + i] = game.die[i];
  out[HELD_AT] = game.held;
  out[ROLLS_AT] = game.rollsUsed;
  out[TURN_AT] = game.turn;
  for (int i = 0; i < 4; ++i) out[RANDOM_AT + i] = static_cast<uint8_t>(game.rng >> (8 * i));
}

bool unpackGame(const uint8_t* data, size_t size, yz::Game& game) {
  if (size != GAME_BYTES) return false;
  yz::Game loaded{};
  if (!unpackCard(data, loaded.card[0]) || !unpackCard(data + CARD_BYTES, loaded.card[1])) return false;
  for (int i = 0; i < yz::kDice; ++i) loaded.die[i] = data[DICE_AT + i];
  loaded.held = data[HELD_AT];
  loaded.rollsUsed = data[ROLLS_AT];
  loaded.turn = data[TURN_AT];
  loaded.rng = 0;
  for (int i = 0; i < 4; ++i) loaded.rng |= static_cast<uint32_t>(data[RANDOM_AT + i]) << (8 * i);
  if (!yz::plausible(loaded) || yz::over(loaded)) return false;
  game = loaded;
  return true;
}

void packRecord(const Record& record, uint8_t* out) {
  put16(out, record.played);
  put16(out + 2, record.won);
  put16(out + 4, record.best);
  put16(out + 6, record.fives);
  out[8] = static_cast<uint8_t>(record.lastFace);
}

bool unpackRecord(const uint8_t* data, size_t size, Record& record) {
  if (size != RECORD_BYTES) return false;
  Record loaded;
  loaded.played = get16(data);
  loaded.won = get16(data + 2);
  loaded.best = get16(data + 4);
  loaded.fives = get16(data + 6);
  loaded.lastFace = data[8];
  if (loaded.won > loaded.played) return false;
  if (loaded.best > MAX_TOTAL || (loaded.best > 0 && loaded.played == 0)) return false;
  if (loaded.lastFace > yz::kFaces) return false;
  record = loaded;
  return true;
}

}  // namespace yachtdice

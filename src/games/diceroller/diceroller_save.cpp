#include "diceroller_save.h"

namespace diceroller {

size_t pack(const Roll& roll, uint8_t* out) {
  for (int d = 0; d < kDice; d++) out[d] = roll.dice[d];
  return SAVE_BYTES;
}

bool unpack(uint8_t version, const uint8_t* in, size_t size, Roll& roll) {
  if (version != SAVE_VERSION || size != SAVE_BYTES) return false;
  for (int d = 0; d < kDice; d++) {
    if (in[d] < 1 || in[d] > kFaces) return false;
  }
  for (int d = 0; d < kDice; d++) roll.dice[d] = in[d];
  return true;
}

}  // namespace diceroller

#include "karubatiles_save.h"

namespace karubatiles {

size_t pack(const State& state, uint8_t* out) {
  out[0] = state.count;
  for (int i = 0; i < state.count; i++) out[SAVE_HEADER + i] = state.order[i];
  return SAVE_HEADER + state.count;
}

bool unpack(uint8_t version, const uint8_t* in, size_t size, State& state) {
  if (version != SAVE_VERSION || size < SAVE_HEADER) return false;
  const uint8_t count = in[0];
  if (count > kTiles || size != SAVE_HEADER + count) return false;

  bool seen[kTiles + 1] = {};
  for (int i = 0; i < count; i++) {
    const uint8_t number = in[SAVE_HEADER + i];
    if (number < 1 || number > kTiles || seen[number]) return false;
    seen[number] = true;
  }

  State loaded{};
  loaded.count = count;
  for (int i = 0; i < count; i++) loaded.order[i] = in[SAVE_HEADER + i];
  state = loaded;
  return true;
}

}  // namespace karubatiles

#include "wordle_save.h"

namespace wordle {

size_t packOpenGame(const OpenGame& game, uint8_t* out) {
  out[0] = static_cast<uint8_t>(game.day & 0xFF);
  out[1] = static_cast<uint8_t>(game.day >> 8);
  out[2] = static_cast<uint8_t>(game.typed);
  for (int i = 0; i < kLetters; i++) {
    out[3 + i] = i < game.typed ? static_cast<uint8_t>(game.letters[i]) : 0;
  }
  return SAVE_BYTES;
}

bool unpackOpenGame(uint8_t version, const uint8_t* in, size_t size, OpenGame& game) {
  if (version != SAVE_VERSION || size != SAVE_BYTES) return false;

  OpenGame loaded;
  loaded.day = in[0] | (in[1] << 8);
  loaded.typed = in[2];
  if (loaded.typed > kLetters) return false;
  for (int i = 0; i < loaded.typed; i++) {
    const uint8_t letter = in[3 + i];
    if (letter < 'A' || letter > 'Z') return false;
    loaded.letters[i] = static_cast<char>(letter);
  }

  game = loaded;
  return true;
}

}  // namespace wordle

#include <cstdio>
#include <cstring>

#include "wordle_save.h"

using namespace wordle;

static int failures = 0;

#define CHECK(cond)                                  \
  do {                                               \
    if (!(cond)) {                                   \
      ++failures;                                    \
      std::printf("FAIL line %d: %s\n", __LINE__, #cond); \
    }                                                \
  } while (0)

int main() {
  OpenGame saved;
  saved.day = 1965;
  saved.typed = 3;
  std::memcpy(saved.letters, "CRA", 3);

  uint8_t data[SAVE_BYTES];
  const size_t size = packOpenGame(saved, data);
  CHECK(size == SAVE_BYTES);

  OpenGame loaded;
  CHECK(unpackOpenGame(SAVE_VERSION, data, size, loaded));
  CHECK(loaded.day == 1965);
  CHECK(loaded.typed == 3);
  CHECK(std::memcmp(loaded.letters, "CRA", 3) == 0);

  OpenGame untouched;
  CHECK(!unpackOpenGame(SAVE_VERSION + 1, data, size, untouched));
  CHECK(!unpackOpenGame(SAVE_VERSION, data, size - 1, untouched));

  uint8_t bad[SAVE_BYTES];
  std::memcpy(bad, data, size);
  bad[2] = kLetters + 1;  // more typed letters than a row holds
  CHECK(!unpackOpenGame(SAVE_VERSION, bad, size, untouched));

  std::memcpy(bad, data, size);
  bad[3] = 'a';  // not a capital
  CHECK(!unpackOpenGame(SAVE_VERSION, bad, size, untouched));
  CHECK(untouched.day == -1);

  std::printf(failures == 0 ? "wordle save: all passed\n" : "wordle save: %d failed\n", failures);
  return failures == 0 ? 0 : 1;
}

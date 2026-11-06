#include <cstdio>
#include <cstring>

#include "oneclue_deck.h"
#include "oneclue_save.h"

using namespace oneclue;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

bool isPermutation(const Deck& deck) {
  bool seen[kCards] = {};
  for (int i = 0; i < kCards; i++) {
    if (deck.order[i] >= kCards || seen[deck.order[i]]) return false;
    seen[deck.order[i]] = true;
  }
  return true;
}

void testWords() {
  for (int c = 0; c < kCards; c++) {
    for (int w = 0; w < kWords; w++) {
      const char* word = CARDS[c][w];
      bool ok = word != nullptr && word[0] != '\0';
      for (const char* p = word; ok && *p; p++) ok = (*p >= 'A' && *p <= 'Z') || *p == '-' || *p == '\'';
      if (!ok) std::printf("card %d word %d\n", c + 1, w + 1);
      check(ok, "every word is capitals");
      for (int o = 0; o < w; o++) check(std::strcmp(word, CARDS[c][o]) != 0, "no card repeats a word");
    }
  }
}

void testShuffle() {
  Deck deck;
  shuffle(deck, 12345);
  check(isPermutation(deck), "a shuffle uses every card once");
  check(deck.drawn == 1, "a shuffle shows one card");

  bool shown[kCards] = {};
  int count = 0;
  do {
    check(!shown[current(deck)], "no card comes back");
    shown[current(deck)] = true;
    count++;
  } while (next(deck));
  check(count == kCards, "the deck shows all its cards");
  check(finished(deck), "the deck ends");
  check(!next(deck) && deck.drawn == kCards, "next stops at the end");

  Deck other;
  shuffle(other, 54321);
  check(std::memcmp(deck.order, other.order, kCards) != 0, "seeds give different orders");

  Deck zero;
  shuffle(zero, 0);
  check(isPermutation(zero), "seed 0 still shuffles");
  int moved = 0;
  for (int i = 0; i < kCards; i++) moved += zero.order[i] != i;
  check(moved > kCards / 2, "seed 0 moves cards");
}

void testSave() {
  Deck deck;
  shuffle(deck, 777);
  for (int i = 0; i < 40; i++) next(deck);

  uint8_t bytes[SAVE_BYTES];
  check(packDeck(deck, bytes) == SAVE_BYTES, "pack size");

  Deck back;
  check(unpackDeck(SAVE_VERSION, bytes, SAVE_BYTES, back), "round trip loads");
  check(std::memcmp(back.order, deck.order, kCards) == 0 && back.drawn == deck.drawn, "round trip matches");

  Deck untouched;
  shuffle(untouched, 1);
  const Deck before = untouched;
  check(!unpackDeck(SAVE_VERSION + 1, bytes, SAVE_BYTES, untouched), "rejects another version");
  check(!unpackDeck(SAVE_VERSION, bytes, SAVE_BYTES - 1, untouched), "rejects a short save");

  uint8_t bad[SAVE_BYTES];
  std::memcpy(bad, bytes, SAVE_BYTES);
  bad[1] = bad[0];
  check(!unpackDeck(SAVE_VERSION, bad, SAVE_BYTES, untouched), "rejects a repeated card");
  std::memcpy(bad, bytes, SAVE_BYTES);
  bad[5] = kCards;
  check(!unpackDeck(SAVE_VERSION, bad, SAVE_BYTES, untouched), "rejects a card past the deck");
  std::memcpy(bad, bytes, SAVE_BYTES);
  bad[kCards] = 0;
  check(!unpackDeck(SAVE_VERSION, bad, SAVE_BYTES, untouched), "rejects no card shown");
  bad[kCards] = kCards + 1;
  check(!unpackDeck(SAVE_VERSION, bad, SAVE_BYTES, untouched), "rejects too many shown");
  bad[kCards] = kCards;
  check(unpackDeck(SAVE_VERSION, bad, SAVE_BYTES, back) && finished(back), "loads a used-up deck");

  check(std::memcmp(untouched.order, before.order, kCards) == 0 && untouched.drawn == before.drawn,
        "a rejected save leaves the deck alone");
}

}  // namespace

int main() {
  testWords();
  testShuffle();
  testSave();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("oneclue: all passed\n");
  return 0;
}

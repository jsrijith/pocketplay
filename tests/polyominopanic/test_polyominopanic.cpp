// Polyomino Panic with no panel: the shapes match the card files, both decks deal each card once
// and then stop, and the save.
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "polyominopanic_cards.h"
#include "polyominopanic_deck.h"
#include "polyominopanic_save.h"

using namespace polyominopanic;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

// Every square reaches every other through side-by-side neighbours.
bool connected(const Shape& s) {
  bool seen[kMaxHeight][kMaxWidth] = {};
  int stack[kMaxWidth * kMaxHeight][2];
  int top = 0, found = 0;
  for (int y = 0; y < height(s) && top == 0; y++) {
    for (int x = 0; x < width(s) && top == 0; x++) {
      if (filled(s, x, y)) {
        seen[y][x] = true;
        stack[top][0] = x;
        stack[top][1] = y;
        top++;
      }
    }
  }
  while (top > 0) {
    top--;
    const int x = stack[top][0], y = stack[top][1];
    found++;
    const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& step : d) {
      const int nx = x + step[0], ny = y + step[1];
      if (nx < 0 || ny < 0 || nx >= kMaxWidth || ny >= kMaxHeight) continue;
      if (seen[ny][nx] || !filled(s, nx, ny)) continue;
      seen[ny][nx] = true;
      stack[top][0] = nx;
      stack[top][1] = ny;
      top++;
    }
  }
  return found == squares(s);
}

// Every row has the same width, so a typo in a shape shows up.
bool rectangular(const Shape& s) {
  int run = 0, first = -1;
  for (const char* p = s.rows;; p++) {
    if (*p == '|' || *p == '\0') {
      if (first < 0) first = run;
      if (run != first) return false;
      run = 0;
      if (*p == '\0') return true;
    } else if (*p == '#' || *p == '.') {
      run++;
    } else {
      return false;
    }
  }
}

void testCards() {
  check(kStarterCount == 13, "13 starter cards");
  check(kGameCount == 40, "40 game cards");
  for (const Shape& s : kStarter) {
    check(squares(s) == 8, "every starter is 8 squares");
    check(rectangular(s) && width(s) <= kMaxWidth && height(s) <= kMaxHeight, "a starter fits the card");
    check(connected(s), "a starter is one piece");
  }
  // The sizes in the card file names, 01 to 40.
  const int sizes[] = {1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5,
                       5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7};
  for (int i = 0; i < kGameCount; i++) {
    check(squares(kGame[i]) == sizes[i], "a game card has the size in its file name");
    check(rectangular(kGame[i]) && width(kGame[i]) <= kMaxWidth && height(kGame[i]) <= kMaxHeight,
          "a game card fits the card");
    check(connected(kGame[i]), "a game card is one piece");
  }
  check(width(kStarter[8]) == 5 && height(kStarter[8]) == 2, "the trident is 5 wide, 2 high");
  check(filled(kStarter[11], 1, 0) && !filled(kStarter[11], 0, 0) && filled(kStarter[11], 0, 3),
        "the pinwheel's squares are where the card has them");
  check(!filled(kGame[0], 1, 0) && !filled(kGame[0], 0, 1) && !filled(kGame[0], -1, 0), "off the shape is empty");
}

void testOrders() {
  for (uint32_t seed = 1; seed < 3000; seed++) {
    for (int count : {kStarterCount, kGameCount}) {
      uint8_t cards[kGameCount];
      order(shuffled(seed), count, cards);
      bool seen[kGameCount] = {};
      bool ok = true;
      for (int i = 0; i < count; i++) {
        ok = ok && cards[i] < count && !seen[cards[i]];
        if (cards[i] < count) seen[cards[i]] = true;
      }
      check(ok, "an order holds every card once");
    }
  }
  check(shuffled(0).seed != 0, "a zero seed is replaced");
}

void testStarter() {
  State s;
  newGame(s, 12345, 678);
  check(starterCard(s) == -1, "nothing on show before the first deal");
  check(shuffleStarter(s, 5) == Change::None, "nothing to shuffle before a deal");
  bool seen[kStarterCount] = {};
  for (int i = 0; i < kStarterCount; i++) {
    check(deal(s) == Change::Redraw, "a deal shows a card");
    const int card = starterCard(s);
    check(card >= 0 && card < kStarterCount && !seen[card], "each deal is a new starter");
    if (card >= 0 && card < kStarterCount) seen[card] = true;
  }
  check(starterEmpty(s) && deal(s) == Change::None, "no deal after all 13");
  check(shuffleStarter(s, 99) == Change::Redraw && s.starter.drawn == 0, "a shuffle puts them all back");
  check(deal(s) == Change::Redraw, "dealing again after a shuffle");
  check(nextRound(s) == Change::None, "no round before play");
}

void testPlay() {
  State s;
  newGame(s, 1, 2);
  deal(s);
  check(startPlay(s) == Change::Full && s.phase == Phase::Play, "start goes to play");
  check(startPlay(s) == Change::None && deal(s) == Change::None, "start and deal do nothing in play");
  check(roundNumber(s) == 1 && cardsLeft(s) == 38, "round 1 has two cards out");
  bool seen[kGameCount] = {};
  for (int r = 1; r <= kRounds; r++) {
    check(roundNumber(s) == r, "rounds count up");
    for (int which = 0; which < kPerRound; which++) {
      const int card = gameCard(s, which);
      check(card >= 0 && card < kGameCount && !seen[card], "each game card turns up once");
      if (card >= 0 && card < kGameCount) seen[card] = true;
    }
    const Change c = nextRound(s);
    check(c == (r == kRounds ? Change::None : r == kRounds - 1 ? Change::Full : Change::Redraw),
          "a round redraws, and the last one refreshes fully");
  }
  check(over(s) && cardsLeft(s) == 0 && roundNumber(s) == kRounds, "the game ends with the deck");
  check(gameCard(s, 0) >= 0, "the last pair stays on show");
  check(newGame(s, 3, 4) == Change::Full && s.phase == Phase::Starter && s.game.drawn == 0 && !over(s),
        "a new game goes back to the starter deck");
}

void testSave() {
  State s;
  newGame(s, 0xDEADBEEF, 0x01020304);
  deal(s);
  deal(s);
  uint8_t data[SAVE_SIZE];
  for (int phase = 0; phase < 2; phase++) {
    check(pack(s, data) == SAVE_SIZE, "pack writes the whole save");
    State back;
    check(unpack(SAVE_VERSION, data, SAVE_SIZE, back), "a save comes back");
    check(back.phase == s.phase && back.starter.seed == s.starter.seed && back.starter.drawn == s.starter.drawn &&
              back.game.seed == s.game.seed && back.game.drawn == s.game.drawn,
          "the round trip keeps everything");
    startPlay(s);
    nextRound(s);
  }

  pack(s, data);
  State back;
  check(!unpack(SAVE_VERSION + 1, data, SAVE_SIZE, back), "a wrong version is refused");
  check(!unpack(SAVE_VERSION, data, SAVE_SIZE - 1, back), "a short save is refused");
  uint8_t bad[SAVE_SIZE];
  std::memcpy(bad, data, SAVE_SIZE);
  bad[0] = 2;
  check(!unpack(SAVE_VERSION, bad, SAVE_SIZE, back), "an unknown phase is refused");
  std::memcpy(bad, data, SAVE_SIZE);
  bad[10] = 5;
  check(!unpack(SAVE_VERSION, bad, SAVE_SIZE, back), "an odd count is refused");
  bad[10] = 42;
  check(!unpack(SAVE_VERSION, bad, SAVE_SIZE, back), "too many game cards is refused");
  bad[10] = 0;
  check(!unpack(SAVE_VERSION, bad, SAVE_SIZE, back), "play with no cards out is refused");
  std::memcpy(bad, data, SAVE_SIZE);
  bad[5] = 14;
  check(!unpack(SAVE_VERSION, bad, SAVE_SIZE, back), "too many starters is refused");
  std::memcpy(bad, data, SAVE_SIZE);
  bad[1] = bad[2] = bad[3] = bad[4] = 0;
  check(!unpack(SAVE_VERSION, bad, SAVE_SIZE, back), "a zero seed is refused");
  std::memcpy(bad, data, SAVE_SIZE);
  bad[0] = 0;
  check(!unpack(SAVE_VERSION, bad, SAVE_SIZE, back), "the starter phase with game cards out is refused");
}

}  // namespace

int main() {
  testCards();
  testOrders();
  testStarter();
  testPlay();
  testSave();
  if (failures) {
    std::printf("polyominopanic: %d failed\n", failures);
    return 1;
  }
  std::printf("polyominopanic: all passed\n");
  return 0;
}

// Yacht Dice played between two devices over FakeLink (tests/link/), as the game
// plays it nearby: CrossPlay's whole yahtzee::Game is sent when a box is taken, the
// rolls stay on the device that rolls them, and each side's computer player chooses.
// Both devices must end on the same game, counted once each.
#include <cstdio>
#include <cstring>

#include "FakeLink.h"
#include "core/link/match.h"
#include "yachtdice_brain.h"

namespace {

using namespace linkplay;
using namespace linktest;
namespace yz = yahtzee;

static_assert(sizeof(yz::Game) == 40, "yahtzee::Game must keep CrossPlay's layout");

int checks = 0;
int failed = 0;

void check(bool ok, const char* what, int line) {
  checks++;
  if (ok) return;
  failed++;
  printf("FAIL  line %d: %s\n", line, what);
}
#define CHECK(expr) check((expr), #expr, __LINE__)

struct Device final : MatchHost {
  // An empty card, not a zeroed one: zero in every box is a full card, and so a
  // finished game.
  Device(Medium& medium, uint8_t last, uint32_t seed) : link(medium, addressOf(last)), play(&link), match(play, *this), seed(seed) {
    yz::start(game, seed);
  }

  void onMatchStart(bool goesFirst) override {
    seat = goesFirst ? 0 : 1;
    yz::start(game, seed);
    starts++;
  }
  bool takeOpponentState() override {
    yz::Game incoming{};
    if (!play.takeOpponent(incoming) || !yz::plausible(incoming)) return false;
    game = incoming;
    return true;
  }
  bool matchGameOver() const override { return yz::over(game); }
  void onMatchEnded() override { ended++; }
  void onLinkEnded() override {}

  // One step of this player's turn, as taps would make it.
  void act() {
    if (!match.yourTurn() || yz::over(game) || game.turn != seat) return;
    const yz::Card& card = game.card[seat];
    if (yz::canRoll(game)) {
      if (game.rollsUsed > 0) game.held = yz::chooseHold(card, game.die);
      yz::roll(game);
      return;
    }
    const yz::Game before = game;
    if (!yz::take(game, yz::chooseBox(card, game.die))) return;
    if (!play.play(game)) game = before;
  }

  FakeTransport link;
  Play<yz::Game> play;
  Match match;
  uint32_t seed;
  yz::Game game{};
  uint8_t seat = 0;
  int starts = 0;
  int ended = 0;
};

bool sameGame(const yz::Game& a, const yz::Game& b) {
  for (int p = 0; p < 2; p++) {
    if (memcmp(a.card[p].box, b.card[p].box, sizeof(a.card[p].box)) != 0) return false;
    if (a.card[p].yahtzeeBonuses != b.card[p].yahtzeeBonuses) return false;
  }
  return a.turn == b.turn && a.rollsUsed == b.rollsUsed;
}

void playMatch(int loss, int duplicate, int jitter, uint32_t seed) {
  Medium medium;
  medium.lossPercent = loss;
  medium.duplicatePercent = duplicate;
  medium.maxJitterMs = jitter;
  medium.random = seed;
  Device a(medium, 1, seed * 7 + 1);
  Device b(medium, 2, seed * 13 + 5);
  CHECK(a.match.enter(GameId::Yahtzee, "A") && b.match.enter(GameId::Yahtzee, "B"));

  for (int pass = 0; pass < 40000 && !(yz::over(a.game) && yz::over(b.game)); pass++) {
    medium.nowMs += 15;
    a.match.update(medium.nowMs);
    b.match.update(medium.nowMs);
    if (pass % 4 == 0) {
      a.act();
      b.act();
    }
    medium.collect();
  }
  // A little longer, for the last acknowledgement and the endgame.
  for (int pass = 0; pass < 400; pass++) {
    medium.nowMs += 15;
    a.match.update(medium.nowMs);
    b.match.update(medium.nowMs);
    medium.collect();
  }

  CHECK(a.starts == 1 && b.starts == 1);
  CHECK(a.seat != b.seat);
  CHECK(yz::over(a.game) && yz::over(b.game));
  CHECK(sameGame(a.game, b.game));
  CHECK(a.ended == 1 && b.ended == 1);
  // Each sees the same two totals, from its own side.
  CHECK(yz::total(a.game.card[a.seat]) == yz::total(b.game.card[1 - b.seat]));
  CHECK(yz::total(a.game.card[1 - a.seat]) == yz::total(b.game.card[b.seat]));
}

}  // namespace

int main() {
  for (uint32_t seed = 1; seed <= 5; seed++) playMatch(0, 0, 0, seed);
  for (uint32_t seed = 1; seed <= 5; seed++) playMatch(25, 15, 50, seed);
  printf("yachtdice nearby: %d checks, %d failed\n", checks, failed);
  return failed == 0 ? 0 : 1;
}

// core/link/match.h: the part of a match that's the same in every game, played
// between two devices over FakeLink. Written for PocketPlay; CrossPlay's LinkActivity,
// which it comes from, has no test of its own because it draws.
#include <cstdio>
#include <cstring>

#include "FakeLink.h"
#include "core/link/match.h"
#include "core/link/radio.h"

namespace {

using namespace linkplay;
using namespace linktest;

int checksRun = 0;
int checksFailed = 0;

void check(const bool condition, const char* what, const int line) {
  checksRun++;
  if (condition) return;
  checksFailed++;
  printf("FAIL  line %d: %s\n", line, what);
}

#define CHECK(expr) check((expr), #expr, __LINE__)

// A game that ends when the counter reaches kEnd.
constexpr uint8_t kEnd = 6;

struct Counter {
  uint8_t count;
};

struct Device final : MatchHost {
  Device(Medium& medium, uint8_t last) : link(medium, addressOf(last)), play(&link), match(play, *this) {}

  void onMatchStart(bool goesFirst) override {
    state = Counter{};
    first = goesFirst;
    starts++;
  }
  bool takeOpponentState() override { return play.takeOpponent(state); }
  bool matchGameOver() const override { return state.count >= kEnd; }
  void onMatchEnded() override { ended++; }
  void onLinkEnded() override { linkEnded++; }

  bool move() {
    if (!match.yourTurn() || matchGameOver()) return false;
    state.count++;
    return play.play(state);
  }

  // One pass of loop(): update, then draw, as the shell does.
  void pass(uint32_t now) {
    match.update(now);
    if (!match.ownsScreen()) match.notePainted(now);
  }

  FakeTransport link;
  Play<Counter> play;
  Match match;
  Counter state{};
  bool first = false;
  int starts = 0;
  int ended = 0;
  int linkEnded = 0;
};

struct World {
  Medium medium;
  Device a{medium, 1};
  Device b{medium, 2};

  void run(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 15) {
      medium.nowMs += 15;
      a.pass(medium.nowMs);
      b.pass(medium.nowMs);
      medium.collect();
    }
  }

  bool pair() {
    if (!a.match.enter(GameId::Test, "A") || !b.match.enter(GameId::Test, "B")) return false;
    run(3000);
    return a.match.inMatch() && b.match.inMatch();
  }

  // Each device moves on its turn until the game ends.
  void playOut() {
    for (int i = 0; i < 200 && !(a.matchGameOver() && b.matchGameOver()); i++) {
      a.move();
      b.move();
      run(600);
    }
  }
};

void pairsAndStarts() {
  World w;
  CHECK(w.pair());
  CHECK(w.a.starts == 1 && w.b.starts == 1);
  CHECK(w.a.first != w.b.first);
  CHECK(!w.a.match.ownsScreen() && !w.b.match.ownsScreen());
  CHECK(w.a.match.keepAwake() && w.b.match.keepAwake());
  CHECK(strcmp(w.a.match.opponentName(), "B") == 0);
  CHECK(w.a.match.linked());
}

void searchingOwnsTheScreen() {
  World w;
  CHECK(w.a.match.enter(GameId::Test, "A"));
  w.run(1000);
  CHECK(w.a.match.ownsScreen());
  CHECK(!w.a.match.linked());
  CHECK(w.a.match.them() == SeatState::Looking);
  CHECK(w.a.match.keepAwake());
  CHECK(!w.a.match.offerPlayAgain());
}

void finishedGameIsHeldThenOffered() {
  World w;
  CHECK(w.pair());
  w.playOut();
  CHECK(w.a.matchGameOver() && w.b.matchGameOver());
  CHECK(w.a.state.count == kEnd && w.b.state.count == kEnd);
  // Counted once on each device, whoever made the last move.
  CHECK(w.a.ended == 1 && w.b.ended == 1);
  // Long after the 2.5 s hold, both are asked.
  w.run(4000);
  CHECK(w.a.match.ownsScreen() && w.b.match.ownsScreen());
  CHECK(w.a.match.offerPlayAgain() && w.b.match.offerPlayAgain());
  CHECK(w.a.match.interruption() == nullptr);
  // A finished game lets the idle timer run.
  CHECK(!w.a.match.keepAwake() && !w.b.match.keepAwake());
}

void finalBoardIsHeldBeforeAsking() {
  World w;
  CHECK(w.pair());
  w.playOut();
  // playOut() runs 600 ms after the last move: still inside the hold.
  CHECK(w.a.ended == 1);
  CHECK(!w.a.match.ownsScreen());
}

void rematchNeedsBoth() {
  World w;
  CHECK(w.pair());
  w.playOut();
  w.run(4000);
  w.a.match.proposeRematch();
  w.run(1500);
  CHECK(w.a.starts == 1 && w.b.starts == 1);  // B hasn't answered
  CHECK(w.a.match.you() == SeatState::Ready);
  CHECK(w.b.match.them() == SeatState::Ready);
  CHECK(w.b.match.you() == SeatState::Deciding);
  w.b.match.proposeRematch();
  w.run(1500);
  CHECK(w.a.starts == 2 && w.b.starts == 2);
  CHECK(w.a.state.count == 0 && w.b.state.count == 0);
  CHECK(!w.a.match.ownsScreen() && !w.b.match.ownsScreen());
  CHECK(w.a.first != w.b.first);
  // And the second game ends and is counted too.
  w.playOut();
  CHECK(w.a.ended == 2 && w.b.ended == 2);
}

void leavingMidGameIsTold() {
  World w;
  CHECK(w.pair());
  w.a.match.leave();
  CHECK(w.a.linkEnded == 1);
  CHECK(!w.a.match.active());
  w.run(1000);
  CHECK(w.b.match.them() == SeatState::Left);
  CHECK(w.b.match.ownsScreen());
  CHECK(!w.b.match.offerPlayAgain());
  CHECK(w.b.match.interruption() != nullptr && strcmp(w.b.match.interruption(), "THEY LEFT") == 0);
  CHECK(!w.b.match.keepAwake());
  // Leaving twice changes nothing.
  w.a.match.leave();
  CHECK(w.a.linkEnded == 1);
}

void decliningAnotherGameIsTold() {
  World w;
  CHECK(w.pair());
  w.playOut();
  w.run(4000);
  w.a.match.proposeRematch();
  w.run(500);
  w.b.match.leave();
  w.run(1000);
  CHECK(w.a.match.them() == SeatState::Left);
  CHECK(!w.a.match.offerPlayAgain());
  CHECK(w.a.starts == 1);
}

void silenceIsLost() {
  World w;
  CHECK(w.pair());
  w.medium.blackholeSource = w.a.link.localAddress();
  w.medium.hasBlackhole = true;
  w.run(12000);
  CHECK(w.b.match.them() == SeatState::Lost);
  CHECK(w.b.match.interruption() != nullptr && strcmp(w.b.match.interruption(), "LOST CONNECTION") == 0);
}

void sleepSaysGoodbye() {
  World w;
  CHECK(w.pair());
  stopForSleep();  // as shell::beforeSleep() does; the last enter() was B's
  CHECK(w.b.linkEnded == 1);
  CHECK(w.a.linkEnded == 0);
  w.run(1000);
  CHECK(w.a.match.them() == SeatState::Left);
}

void enteringAgainStartsClean() {
  World w;
  CHECK(w.pair());
  w.a.match.leave();
  w.b.match.leave();
  w.run(500);
  CHECK(w.pair());
  CHECK(w.a.starts == 2 && w.b.starts == 2);
  CHECK(w.a.match.them() == SeatState::Deciding);
  CHECK(!w.a.match.ownsScreen());
}

void survivesABadLink() {
  World w;
  w.medium.lossPercent = 30;
  w.medium.duplicatePercent = 20;
  // As bad as CrossPlay's own tests make it. Much more jitter than this can bring a
  // stale Hello in after pairing, which the session reads, by design, as the other
  // device having left to look again.
  w.medium.maxJitterMs = 50;
  CHECK(w.a.match.enter(GameId::Test, "A") && w.b.match.enter(GameId::Test, "B"));
  w.run(8000);
  CHECK(w.a.match.inMatch() && w.b.match.inMatch());
  w.playOut();
  CHECK(w.a.state.count == kEnd && w.b.state.count == kEnd);
  CHECK(w.a.ended == 1 && w.b.ended == 1);
}

}  // namespace

int main() {
  pairsAndStarts();
  searchingOwnsTheScreen();
  finishedGameIsHeldThenOffered();
  finalBoardIsHeldBeforeAsking();
  rematchNeedsBoth();
  leavingMidGameIsTold();
  decliningAnotherGameIsTold();
  silenceIsLost();
  sleepSaysGoodbye();
  enteringAgainStartsClean();
  survivesABadLink();
  printf("test_match: %d checks, %d failed\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}

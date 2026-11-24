// Yacht Dice's save packing (yachtdice_save.h).
#include <cstdio>
#include <cstring>

#include "yachtdice_brain.h"
#include "yachtdice_save.h"

namespace {

namespace yd = yachtdice;
namespace yz = yahtzee;

int checks = 0;
int failed = 0;

void check(bool ok, const char* what, int line) {
  checks++;
  if (ok) return;
  failed++;
  printf("FAIL  line %d: %s\n", line, what);
}
#define CHECK(expr) check((expr), #expr, __LINE__)

bool sameGame(const yz::Game& a, const yz::Game& b) {
  for (int p = 0; p < 2; p++) {
    if (memcmp(a.card[p].box, b.card[p].box, sizeof(a.card[p].box)) != 0) return false;
    if (a.card[p].yahtzeeBonuses != b.card[p].yahtzeeBonuses) return false;
  }
  return memcmp(a.die, b.die, sizeof(a.die)) == 0 && a.held == b.held && a.rollsUsed == b.rollsUsed &&
         a.turn == b.turn && a.rng == b.rng;
}

// One step of the computer's play, for either seat, as the game plays its own turn.
void step(yz::Game& game) {
  const yz::Card& card = game.card[game.turn];
  if (yz::canRoll(game)) {
    if (game.rollsUsed > 0) game.held = yz::chooseHold(card, game.die);
    yz::roll(game);
    return;
  }
  yz::take(game, yz::chooseBox(card, game.die));
}

void roundTripsEveryStepOfAGame() {
  yz::Game game{};
  yz::start(game, 12345);
  uint8_t data[yd::GAME_BYTES];
  int steps = 0;
  while (!yz::over(game) && steps < 1000) {
    yd::packGame(game, data);
    yz::Game back{};
    CHECK(yd::unpackGame(data, sizeof(data), back));
    CHECK(sameGame(game, back));
    step(game);
    steps++;
  }
  CHECK(yz::over(game));
  // A finished game is never saved, so it never loads.
  yd::packGame(game, data);
  yz::Game back{};
  CHECK(!yd::unpackGame(data, sizeof(data), back));
}

void refusesBadGames() {
  yz::Game game{};
  yz::start(game, 99);
  yz::roll(game);
  uint8_t good[yd::GAME_BYTES];
  yd::packGame(game, good);
  yz::Game out{};
  CHECK(yd::unpackGame(good, sizeof(good), out));
  CHECK(!yd::unpackGame(good, sizeof(good) - 1, out));

  uint8_t bad[yd::GAME_BYTES];
  memcpy(bad, good, sizeof(bad));
  bad[28] = 7;  // a die past 6
  CHECK(!yd::unpackGame(bad, sizeof(bad), out));

  memcpy(bad, good, sizeof(bad));
  bad[0] = 4;  // a legal score, but your card would be a box ahead on your own turn
  CHECK(!yd::unpackGame(bad, sizeof(bad), out));

  memcpy(bad, good, sizeof(bad));
  bad[8] = 20;  // a full house of 20
  bad[14 + 8] = 25;
  CHECK(!yd::unpackGame(bad, sizeof(bad), out));

  memcpy(bad, good, sizeof(bad));
  bad[0] = 200;  // not a score and not the empty mark
  CHECK(!yd::unpackGame(bad, sizeof(bad), out));

  memcpy(bad, good, sizeof(bad));
  bad[35] = 2;  // a third player
  CHECK(!yd::unpackGame(bad, sizeof(bad), out));

  memcpy(bad, good, sizeof(bad));
  bad[34] = 4;  // a fourth roll
  CHECK(!yd::unpackGame(bad, sizeof(bad), out));

  // A refused load leaves the game as it was.
  yz::Game kept = game;
  memcpy(bad, good, sizeof(bad));
  bad[28] = 0;
  CHECK(!yd::unpackGame(bad, sizeof(bad), kept));
  CHECK(sameGame(kept, game));
}

void roundTripsTheRecord() {
  yd::Record record;
  record.played = 300;
  record.won = 120;
  record.best = 412;
  record.fives = 37;
  record.lastFace = 6;
  uint8_t data[yd::RECORD_BYTES];
  yd::packRecord(record, data);
  yd::Record back;
  CHECK(yd::unpackRecord(data, sizeof(data), back));
  CHECK(back.played == 300 && back.won == 120 && back.best == 412 && back.fives == 37 && back.lastFace == 6);

  yd::Record empty;
  yd::packRecord(empty, data);
  CHECK(yd::unpackRecord(data, sizeof(data), back));
  CHECK(back.played == 0 && back.lastFace == 0);
}

void refusesBadRecords() {
  yd::Record record;
  uint8_t data[yd::RECORD_BYTES];
  yd::Record out;

  record.played = 2;
  record.won = 3;
  yd::packRecord(record, data);
  CHECK(!yd::unpackRecord(data, sizeof(data), out));

  record = yd::Record{};
  record.best = 100;  // a best with nothing played
  yd::packRecord(record, data);
  CHECK(!yd::unpackRecord(data, sizeof(data), out));

  record.played = 1;
  record.best = yd::MAX_TOTAL + 1;
  yd::packRecord(record, data);
  CHECK(!yd::unpackRecord(data, sizeof(data), out));

  record.best = 200;
  record.lastFace = 7;
  yd::packRecord(record, data);
  CHECK(!yd::unpackRecord(data, sizeof(data), out));

  record.lastFace = 3;
  yd::packRecord(record, data);
  CHECK(!yd::unpackRecord(data, sizeof(data) - 1, out));
  CHECK(yd::unpackRecord(data, sizeof(data), out));
}

}  // namespace

int main() {
  roundTripsEveryStepOfAGame();
  refusesBadGames();
  roundTripsTheRecord();
  refusesBadRecords();
  printf("yachtdice save: %d checks, %d failed\n", checks, failed);
  return failed == 0 ? 0 : 1;
}

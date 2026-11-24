// Yacht Dice, ported from CrossPlay's YahtzeeActivity.cpp, against the built-in
// opponent. Roll, tap dice to keep them, roll again, then tap a box on your card.
#include "yachtdice.h"

#include <Arduino.h>
#include <esp_random.h>

#include "core/storage.h"
#include "ui/ui.h"
#include "yachtdice_brain.h"
#include "yachtdice_flow.h"
#include "yachtdice_icon.h"
#include "yachtdice_rules.h"
#include "yachtdice_save.h"
#include "yachtdice_screens.h"

namespace {

namespace yz = yahtzee;
namespace yd = yachtdice;

constexpr const char* GAME_FILE = "yachtdice";
constexpr const char* RECORD_FILE = "yachtdice-record";
constexpr int MAX_TALLY = 0xFFFF;  // the record keeps each count in two bytes

// You always play seat 0, and the built-in opponent seat 1.
constexpr uint8_t YOU = 0;
constexpr uint8_t THEM = 1;

// The opponent's rolls and its box are each shown this long before the next, so
// its turn can be followed rather than jumping to the end.
constexpr unsigned long STEP_MS = 600;

bool fiveEqual(const uint8_t* die) {
  for (int i = 1; i < yz::kDice; ++i) {
    if (die[i] != die[0]) return false;
  }
  return true;
}

class YachtDice final : public Game {
 public:
  const char* name() const override { return "YACHT DICE"; }
  const uint8_t* icon() const override { return yachtdiceIcon::ICON; }

  // The menu every time, as in CrossPlay, with a game in play offered as RESUME.
  void open() override {
    if (!loaded_) {
      loadGame();
      loadRecord();
      loaded_ = true;
    }
    screen_ = yz::Screen::Menu;
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) {
      if (yz::leavesApp(screen_)) return TapResult::Leave;
      screen_ = yz::back(screen_);
      return TapResult::Redraw;
    }
    switch (screen_) {
      case yz::Screen::Menu:
        return tapMenu(x, y);
      case yz::Screen::HowTo:
        return tapHowTo(x, y);
      case yz::Screen::Card:
        return tapCard(x, y);
      case yz::Screen::Result:
        return tapResult(x, y);
    }
    return TapResult::Nothing;
  }

  // The how-to's pages on the side buttons, as CrossPlay's.
  TapResult onUpPressed() override { return turnPage(-1); }
  TapResult onDownPressed() override { return turnPage(1); }

  // The opponent's turn, one roll or box per call, each left on screen for STEP_MS.
  // The game ending moves to the result on the call after, so its last box is seen.
  bool update() override {
    if (screen_ != yz::Screen::Card || !started_) return false;
    const unsigned long now = millis();
    if (now - lastStepMs_ < STEP_MS) return false;
    if (yz::over(game_)) {
      screen_ = yz::Screen::Result;
      return true;
    }
    if (game_.turn != THEM) return false;
    const yz::Card& card = game_.card[THEM];
    if (yz::canRoll(game_)) {
      if (game_.rollsUsed > 0) game_.held = yz::chooseHold(card, game_.die);
      yz::roll(game_);
    } else if (!yz::take(game_, yz::chooseBox(card, game_.die))) {
      return false;
    }
    lastStepMs_ = now;
    if (yz::over(game_)) settle();
    persist();
    return true;
  }

  void draw(uint8_t* frame) override {
    switch (screen_) {
      case yz::Screen::Menu:
        yachtui::drawMenu(frame, menuModel());
        break;
      case yz::Screen::HowTo:
        yachtui::drawHowTo(frame, howToPage_);
        break;
      case yz::Screen::Card: {
        yachtui::CardModel model;
        model.game = &game_;
        model.seat = YOU;
        model.yourTurn = game_.turn == YOU && !yz::over(game_);
        model.takeable = yz::takeableBoxes(game_, YOU);
        model.joker = yz::jokerForcing(game_, YOU);
        yachtui::drawCard(frame, model);
        break;
      }
      case yz::Screen::Result: {
        yachtui::ResultModel model;
        model.yours = &game_.card[YOU];
        model.theirs = &game_.card[THEM];
        yachtui::drawResult(frame, model);
        break;
      }
    }
  }

  bool saveFailed() const override { return gameFailed_ || recordFailed_; }

  void retrySave() override {
    if (gameFailed_) persist();
    if (recordFailed_) saveRecord();
  }

 private:
  bool inProgress() const { return started_ && !yz::over(game_); }

  yachtui::MenuModel menuModel() const {
    yachtui::MenuModel model;
    model.inProgress = inProgress();
    model.played = record_.played;
    model.won = record_.won;
    model.best = record_.best;
    model.fives = record_.fives;
    model.lastFace = record_.lastFace;
    return model;
  }

  TapResult tapMenu(int x, int y) {
    switch (yachtui::menuTapAt(menuModel(), x, y)) {
      case yachtui::MenuTap::Nothing:
        return TapResult::Nothing;
      case yachtui::MenuTap::Resume:
        screen_ = yz::Screen::Card;
        lastStepMs_ = millis();
        return TapResult::RedrawFull;
      case yachtui::MenuTap::Play:
      case yachtui::MenuTap::NewGame:
        return beginGame();
      case yachtui::MenuTap::HowTo:
        howToPage_ = 0;
        screen_ = yz::Screen::HowTo;
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult tapHowTo(int x, int y) {
    if (!yachtui::isOnHowToNext(x, y)) return TapResult::Nothing;
    if (howToPage_ + 1 < yachtui::howToPages()) {
      ++howToPage_;
    } else {
      screen_ = yz::Screen::Menu;
    }
    return TapResult::Redraw;
  }

  TapResult turnPage(int step) {
    if (screen_ != yz::Screen::HowTo) return TapResult::Nothing;
    const int pages = yachtui::howToPages();
    howToPage_ = (howToPage_ + step + pages) % pages;
    return TapResult::Redraw;
  }

  // The rules decide what a tap may do: a die can't be held before the first roll
  // or after the last, and a box refuses outside its turn or against the Joker. A
  // refused tap draws nothing.
  TapResult tapCard(int x, int y) {
    const bool mine = game_.turn == YOU && !yz::over(game_);
    const int die = yachtui::dieAt(x, y);
    if (die >= 0) {
      if (!mine || !yz::toggleHold(game_, die)) return TapResult::Nothing;
      persist();
      return TapResult::Redraw;
    }
    const int category = yachtui::categoryAt(x, y);
    if (category >= 0) {
      if (!mine) return TapResult::Nothing;
      // Five equal dice in any box were rolled as five of a kind, and the face is
      // what the menu draws.
      const bool five = fiveEqual(game_.die);
      const uint8_t face = game_.die[0];
      if (!yz::take(game_, static_cast<yz::Category>(category))) return TapResult::Nothing;
      if (five) lastFace_ = face;
      lastStepMs_ = millis();
      persist();
      return TapResult::Redraw;
    }
    if (yachtui::isOnAction(x, y)) {
      if (!mine || !yz::roll(game_)) return TapResult::Nothing;
      persist();
      return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult tapResult(int x, int y) {
    switch (yachtui::resultTapAt(x, y)) {
      case yachtui::ResultTap::Nothing:
        return TapResult::Nothing;
      case yachtui::ResultTap::Again:
        return beginGame();
      case yachtui::ResultTap::Done:
        screen_ = yz::Screen::Menu;
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult beginGame() {
    // The chip's hardware random numbers. The rules take a seed and never reach for a
    // clock, as in CrossPlay.
    uint32_t seed = esp_random();
    if (seed == 0) seed = 1;
    yz::start(game_, seed);
    started_ = true;
    recorded_ = false;
    lastFace_ = 0;
    screen_ = yz::Screen::Card;
    persist();
    return TapResult::RedrawFull;
  }

  // Written the moment the last box ends the game, so leaving before the result
  // still counts it, and only once. The persist() after it erases the game save.
  void settle() {
    if (recorded_) return;
    recorded_ = true;
    const yz::Card& yours = game_.card[YOU];
    const int mine = yz::total(yours);
    if (record_.played < MAX_TALLY) ++record_.played;
    if (mine > yz::total(game_.card[THEM]) && record_.won < MAX_TALLY) ++record_.won;
    if (mine > record_.best) record_.best = mine;
    // Every five of a kind this game: the box when it scored 50, and each bonus.
    int fives = yours.yahtzeeBonuses;
    if (yours.box[static_cast<int>(yz::Category::Yahtzee)] == yz::kYahtzeeScore) ++fives;
    record_.fives = record_.fives + fives < MAX_TALLY ? record_.fives + fives : MAX_TALLY;
    if (lastFace_ != 0) record_.lastFace = lastFace_;
    saveRecord();
  }

  // --- Saving ---

  void persist() {
    if (!storage::ready()) return;
    if (!inProgress()) {
      gameFailed_ = !storage::erase(GAME_FILE);
      return;
    }
    uint8_t data[yd::GAME_BYTES];
    yd::packGame(game_, data);
    gameFailed_ = !storage::save(GAME_FILE, yd::GAME_VERSION, data, sizeof(data));
  }

  void loadGame() {
    if (!storage::ready()) return;
    uint8_t data[yd::GAME_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(GAME_FILE, version, data, sizeof(data), size)) return;
    if (version != yd::GAME_VERSION || !yd::unpackGame(data, size, game_)) {
      Serial.println("Yacht Dice: the game didn't check out, so it's set aside");
      return;
    }
    started_ = true;
  }

  void saveRecord() {
    if (!storage::ready()) return;
    uint8_t data[yd::RECORD_BYTES];
    yd::packRecord(record_, data);
    recordFailed_ = !storage::save(RECORD_FILE, yd::RECORD_VERSION, data, sizeof(data));
  }

  void loadRecord() {
    if (!storage::ready()) return;
    uint8_t data[yd::RECORD_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(RECORD_FILE, version, data, sizeof(data), size)) return;
    if (version != yd::RECORD_VERSION || !yd::unpackRecord(data, size, record_)) {
      Serial.println("Yacht Dice: the record didn't check out, so it starts again");
    }
  }

  yz::Game game_{};
  yz::Screen screen_ = yz::Screen::Menu;
  bool loaded_ = false;
  bool started_ = false;   // game_ is a game the player began, not the empty default
  bool recorded_ = false;  // this game's ending is in the record
  // The face of your last five of a kind this game, or 0, for the menu's ornament.
  // Not in the game save, so a game resumed after a restart forgets one rolled
  // before it; the count comes from the card and is kept.
  uint8_t lastFace_ = 0;
  unsigned long lastStepMs_ = 0;
  int howToPage_ = 0;
  yd::Record record_;
  bool gameFailed_ = false;
  bool recordFailed_ = false;
};

YachtDice instance;

}  // namespace

Game& yachtDiceGame() { return instance; }

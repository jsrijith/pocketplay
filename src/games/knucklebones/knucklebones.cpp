// Knucklebones, ported from CrossPlay's KnucklebonesActivity.cpp, against the
// built-in opponent. Tap one of your columns to drop the die into it.
#include "knucklebones.h"

#include <Arduino.h>
#include <esp_random.h>

#include "core/storage.h"
#include "knucklebones_brain.h"
#include "knucklebones_flow.h"
#include "knucklebones_icon.h"
#include "knucklebones_rules.h"
#include "knucklebones_save.h"
#include "knucklebones_screens.h"
#include "ui/ui.h"

namespace {

namespace kb = knucklebones;

constexpr const char* MATCH_FILE = "knucklebones";
constexpr const char* RECORD_FILE = "knucklebones-record";
constexpr int MAX_TALLY = 0xFFFF;  // the record keeps each count in two bytes

// You always play seat 0, and the built-in opponent seat 1.
constexpr int YOU = 0;
constexpr int THEM = 1;

class Knucklebones final : public Game {
 public:
  const char* name() const override { return "KNUCKLEBONES"; }
  const uint8_t* icon() const override { return knucklebonesIcon::ICON; }

  // The menu every time, as in CrossPlay, with a match in play offered as RESUME.
  void open() override {
    if (!loaded_) {
      loadMatch();
      loadRecord();
      loaded_ = true;
    }
    screen_ = kb::Screen::Menu;
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) {
      if (kb::leavesApp(screen_)) return TapResult::Leave;
      screen_ = kb::back(screen_);
      return TapResult::Redraw;
    }
    switch (screen_) {
      case kb::Screen::Menu:
        return tapMenu(x, y);
      case kb::Screen::HowTo:
        return tapHowTo(x, y);
      case kb::Screen::Board:
        return tapBoard(x, y);
      case kb::Screen::Result:
        return tapResult(x, y);
    }
    return TapResult::Nothing;
  }

  // The opponent's turn, one placement per call, as CrossPlay plays it once per
  // pass: each placement is drawn before the next, so nothing seems to skip. The
  // match ending moves to the result on the call after, so the last die is seen
  // landing first.
  bool update() override {
    if (screen_ != kb::Screen::Board || !started_) return false;
    if (kb::over(game_)) {
      screen_ = kb::Screen::Result;
      return true;
    }
    if (game_.turn != THEM) return false;
    const int column = kb::chooseColumn(game_);
    if (column < 0 || !kb::place(game_, column)) return false;
    if (kb::over(game_)) settle();
    persist();
    return true;
  }

  void draw(uint8_t* frame) override {
    switch (screen_) {
      case kb::Screen::Menu: {
        knuckleui::MenuModel model = menuModel();
        model.hasHistory = record_.hasLast;
        model.lastYours = &record_.lastYours;
        model.lastTheirs = &record_.lastTheirs;
        model.wins = record_.wins;
        model.losses = record_.losses;
        model.draws = record_.draws;
        knuckleui::drawMenu(frame, model);
        break;
      }
      case kb::Screen::HowTo:
        knuckleui::drawHowTo(frame, howToPage_);
        break;
      case kb::Screen::Board: {
        knuckleui::BoardModel model;
        model.yours = &game_.grid[YOU];
        model.theirs = &game_.grid[THEM];
        model.die = game_.die;
        model.yourTurn = game_.turn == YOU;
        knuckleui::drawBoard(frame, model);
        break;
      }
      case kb::Screen::Result: {
        knuckleui::ResultModel model;
        model.yourScore = kb::score(game_.grid[YOU]);
        model.theirScore = kb::score(game_.grid[THEM]);
        knuckleui::drawResult(frame, model);
        break;
      }
    }
  }

  bool saveFailed() const override { return matchFailed_ || recordFailed_; }

  void retrySave() override {
    if (matchFailed_) persist();
    if (recordFailed_) saveRecord();
  }

 private:
  bool inProgress() const { return started_ && !kb::over(game_); }

  knuckleui::MenuModel menuModel() const {
    knuckleui::MenuModel model;
    model.inProgress = inProgress();
    return model;
  }

  TapResult tapMenu(int x, int y) {
    switch (knuckleui::menuTapAt(menuModel(), x, y)) {
      case knuckleui::MenuTap::Nothing:
        return TapResult::Nothing;
      case knuckleui::MenuTap::Resume:
        screen_ = kb::Screen::Board;
        return TapResult::RedrawFull;
      case knuckleui::MenuTap::Play:
      case knuckleui::MenuTap::NewGame:
        return beginMatch();
      case knuckleui::MenuTap::HowTo:
        howToPage_ = 0;
        screen_ = kb::Screen::HowTo;
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult tapHowTo(int x, int y) {
    if (!knuckleui::isOnHowToNext(x, y)) return TapResult::Nothing;
    if (howToPage_ + 1 < knuckleui::howToPages()) {
      ++howToPage_;
    } else {
      screen_ = kb::Screen::Menu;
    }
    return TapResult::Redraw;
  }

  // The rules decide whether a placement is legal: a full column, the opponent's
  // turn or a finished match places nothing, and draws nothing.
  TapResult tapBoard(int x, int y) {
    const int column = knuckleui::columnAt(x, y);
    if (column < 0 || game_.turn != YOU || !kb::place(game_, column)) return TapResult::Nothing;
    if (kb::over(game_)) settle();
    persist();
    return TapResult::Redraw;
  }

  TapResult tapResult(int x, int y) {
    switch (knuckleui::resultTapAt(x, y)) {
      case knuckleui::ResultTap::Nothing:
        return TapResult::Nothing;
      case knuckleui::ResultTap::Again:
        return beginMatch();
      case knuckleui::ResultTap::Done:
        screen_ = kb::Screen::Menu;
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult beginMatch() {
    // The chip's hardware random numbers. The rules take a seed and never reach for
    // a clock, as in CrossPlay. Zero would be replaced by a constant, so it's avoided.
    uint32_t seed = esp_random();
    if (seed == 0) seed = 1;
    kb::start(game_, seed);
    started_ = true;
    recorded_ = false;
    screen_ = kb::Screen::Board;
    persist();
    return TapResult::RedrawFull;
  }

  // Written the moment a placement ends the match, so leaving before the result
  // still counts it, and only once. The persist() after it erases the match save,
  // so the menu doesn't offer it again.
  void settle() {
    if (recorded_) return;
    recorded_ = true;
    record_.lastYours = game_.grid[YOU];
    record_.lastTheirs = game_.grid[THEM];
    record_.hasLast = true;
    const int mine = kb::score(game_.grid[YOU]);
    const int theirs = kb::score(game_.grid[THEM]);
    int& tally = mine > theirs ? record_.wins : (theirs > mine ? record_.losses : record_.draws);
    if (tally < MAX_TALLY) ++tally;
    saveRecord();
  }

  // --- Saving ---

  void persist() {
    if (!storage::ready()) return;
    if (!inProgress()) {
      matchFailed_ = !storage::erase(MATCH_FILE);
      return;
    }
    uint8_t data[kb::MATCH_BYTES];
    kb::packMatch(game_, data);
    matchFailed_ = !storage::save(MATCH_FILE, kb::MATCH_VERSION, data, sizeof(data));
  }

  void loadMatch() {
    if (!storage::ready()) return;
    uint8_t data[kb::MATCH_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(MATCH_FILE, version, data, sizeof(data), size)) return;
    if (version != kb::MATCH_VERSION || !kb::unpackMatch(data, size, game_)) {
      Serial.println("Knucklebones: the match didn't check out, so it's set aside");
      return;
    }
    started_ = true;
  }

  void saveRecord() {
    if (!storage::ready()) return;
    uint8_t data[kb::RECORD_BYTES];
    kb::packRecord(record_, data);
    recordFailed_ = !storage::save(RECORD_FILE, kb::RECORD_VERSION, data, sizeof(data));
  }

  void loadRecord() {
    if (!storage::ready()) return;
    uint8_t data[kb::RECORD_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(RECORD_FILE, version, data, sizeof(data), size)) return;
    if (version != kb::RECORD_VERSION || !kb::unpackRecord(data, size, record_)) {
      Serial.println("Knucklebones: the record didn't check out, so it starts again");
    }
  }

  kb::Game game_{};
  kb::Screen screen_ = kb::Screen::Menu;
  bool loaded_ = false;
  bool started_ = false;   // game_ is a match the player began, not the empty default
  bool recorded_ = false;  // this match's ending is in the record
  int howToPage_ = 0;
  kb::Record record_;
  bool matchFailed_ = false;
  bool recordFailed_ = false;
};

Knucklebones instance;

}  // namespace

Game& knucklebonesGame() { return instance; }

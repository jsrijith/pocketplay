// Connect Four, ported from CrossPlay's ConnectFourActivity.cpp, against the
// built-in opponent. Tap a column to drop your disc down it.
#include "connectfour.h"

#include <Arduino.h>

#include "connectfour_brain.h"
#include "connectfour_flow.h"
#include "connectfour_icon.h"
#include "connectfour_rules.h"
#include "connectfour_save.h"
#include "connectfour_screens.h"
#include "core/storage.h"
#include "ui/ui.h"

namespace {

namespace c4 = connectfour;

constexpr const char* GAME_FILE = "connectfour";
constexpr const char* RECORD_FILE = "connectfour-record";
constexpr int MAX_TALLY = 0xFFFF;  // the record keeps each count in two bytes

// You always play light, which opens, as in CrossPlay's solo game.
constexpr uint8_t YOU = c4::kLight;

class ConnectFour final : public Game {
 public:
  const char* name() const override { return "CONNECT FOUR"; }
  const uint8_t* icon() const override { return connectfourIcon::ICON; }

  // The menu every time, as in CrossPlay, with a game in play offered as RESUME.
  void open() override {
    if (!loaded_) {
      loadGame();
      loadRecord();
      loaded_ = true;
    }
    screen_ = c4::Screen::Menu;
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) {
      if (c4::leavesApp(screen_)) return TapResult::Leave;
      screen_ = c4::back(screen_);
      return TapResult::Redraw;
    }
    switch (screen_) {
      case c4::Screen::Menu:
        return tapMenu(x, y);
      case c4::Screen::HowTo:
        return tapHowTo(x, y);
      case c4::Screen::Board:
        return tapBoard(x, y);
      case c4::Screen::Result:
        return tapResult(x, y);
    }
    return TapResult::Nothing;
  }

  // How to play also turns with the buttons, as CrossPlay's does: Down forward and
  // Up back, wrapping round. Nowhere else uses them.
  TapResult onUpPressed() override { return stepHowTo(-1); }
  TapResult onDownPressed() override { return stepHowTo(1); }

  // The opponent's disc, one per call, as CrossPlay plays it once per pass, so
  // your disc is drawn before it thinks. The game settling moves to the result on
  // the call after, so the last disc is seen landing first.
  bool update() override {
    if (screen_ != c4::Screen::Board || !started_) return false;
    if (c4::over(game_)) {
      screen_ = c4::Screen::Result;
      return true;
    }
    if (game_.turn == YOU) return false;
    const int column = c4::chooseColumn(game_);
    if (column == c4::kNoColumn || !c4::drop(game_, column)) return false;
    if (c4::over(game_)) settle();
    persist();
    return true;
  }

  void draw(uint8_t* frame) override {
    switch (screen_) {
      case c4::Screen::Menu: {
        c4ui::MenuModel model = menuModel();
        model.hasHistory = record_.hasLast;
        model.wins = record_.wins;
        model.losses = record_.losses;
        model.draws = record_.draws;
        model.lastOutcome = record_.lastOutcome;
        model.lastCells = record_.lastCells;
        model.lastLine = record_.lastLine;
        c4ui::drawMenu(frame, model);
        break;
      }
      case c4::Screen::HowTo:
        c4ui::drawHowTo(frame, howToPage_);
        break;
      case c4::Screen::Board: {
        c4ui::BoardModel model;
        model.game = &game_;
        model.yourTurn = game_.turn == YOU;
        c4ui::drawBoard(frame, model);
        break;
      }
      case c4::Screen::Result: {
        // The four that won come from the rules' own search, so the mark can't
        // disagree with the outcome.
        int line[c4::kLine] = {-1, -1, -1, -1};
        c4ui::ResultModel model;
        model.game = &game_;
        if (game_.outcome == c4::Outcome::LightWins && c4::winningLine(game_, c4::kLight, line)) model.line = line;
        if (game_.outcome == c4::Outcome::DarkWins && c4::winningLine(game_, c4::kDark, line)) model.line = line;
        c4ui::drawResult(frame, model);
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
  bool inProgress() const { return started_ && !c4::over(game_); }

  c4ui::MenuModel menuModel() const {
    c4ui::MenuModel model;
    model.inProgress = inProgress();
    return model;
  }

  TapResult tapMenu(int x, int y) {
    switch (c4ui::menuTapAt(menuModel(), x, y)) {
      case c4ui::MenuTap::Nothing:
        return TapResult::Nothing;
      case c4ui::MenuTap::Resume:
        screen_ = c4::Screen::Board;
        return TapResult::RedrawFull;
      case c4ui::MenuTap::Play:
      case c4ui::MenuTap::NewGame:
        return beginGame();
      case c4ui::MenuTap::HowTo:
        howToPage_ = 0;
        screen_ = c4::Screen::HowTo;
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult stepHowTo(int step) {
    if (screen_ != c4::Screen::HowTo) return TapResult::Nothing;
    const int pages = c4ui::howToPages();
    howToPage_ = (howToPage_ + step + pages) % pages;
    return TapResult::Redraw;
  }

  TapResult tapHowTo(int x, int y) {
    if (!c4ui::isOnHowToNext(x, y)) return TapResult::Nothing;
    if (howToPage_ + 1 < c4ui::howToPages()) {
      ++howToPage_;
    } else {
      screen_ = c4::Screen::Menu;
    }
    return TapResult::Redraw;
  }

  // The rules decide: a full column, the opponent's turn or a settled game drops
  // nothing, and draws nothing.
  TapResult tapBoard(int x, int y) {
    const int column = c4ui::columnAt(x, y);
    if (column == c4::kNoColumn || game_.turn != YOU || !c4::drop(game_, column)) return TapResult::Nothing;
    if (c4::over(game_)) settle();
    persist();
    return TapResult::Redraw;
  }

  TapResult tapResult(int x, int y) {
    switch (c4ui::resultTapAt(x, y)) {
      case c4ui::ResultTap::Nothing:
        return TapResult::Nothing;
      case c4ui::ResultTap::Again:
        return beginGame();
      case c4ui::ResultTap::Done:
        screen_ = c4::Screen::Menu;
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult beginGame() {
    c4::start(game_);
    started_ = true;
    recorded_ = false;
    screen_ = c4::Screen::Board;
    persist();
    return TapResult::RedrawFull;
  }

  // Written the moment a disc settles the game, so leaving before the result still
  // counts it, and only once. The persist() after it erases the game save, so the
  // menu doesn't offer it again.
  void settle() {
    if (recorded_) return;
    recorded_ = true;
    int& tally = game_.outcome == c4::Outcome::Draw ? record_.draws
                 : game_.outcome == c4::Outcome::LightWins ? record_.wins
                                                           : record_.losses;
    if (tally < MAX_TALLY) ++tally;
    record_.hasLast = true;
    record_.lastOutcome = game_.outcome == c4::Outcome::Draw        ? c4::LAST_DRAW
                          : game_.outcome == c4::Outcome::LightWins ? c4::LAST_WON
                                                                    : c4::LAST_LOST;
    for (int i = 0; i < c4::kLine; ++i) record_.lastLine[i] = -1;
    if (game_.outcome == c4::Outcome::LightWins) c4::winningLine(game_, c4::kLight, record_.lastLine);
    if (game_.outcome == c4::Outcome::DarkWins) c4::winningLine(game_, c4::kDark, record_.lastLine);
    for (int column = 0; column < c4::kColumns; ++column) {
      for (int row = 0; row < c4::kRows; ++row) record_.lastCells[column * c4::kRows + row] = game_.cell[column][row];
    }
    saveRecord();
  }

  // --- Saving ---

  void persist() {
    if (!storage::ready()) return;
    if (!inProgress()) {
      gameFailed_ = !storage::erase(GAME_FILE);
      return;
    }
    uint8_t data[c4::GAME_BYTES];
    c4::packGame(game_, data);
    gameFailed_ = !storage::save(GAME_FILE, c4::GAME_VERSION, data, sizeof(data));
  }

  void loadGame() {
    if (!storage::ready()) return;
    uint8_t data[c4::GAME_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(GAME_FILE, version, data, sizeof(data), size)) return;
    if (version != c4::GAME_VERSION || !c4::unpackGame(data, size, game_)) {
      Serial.println("Connect Four: the game didn't check out, so it's set aside");
      return;
    }
    started_ = true;
  }

  void saveRecord() {
    if (!storage::ready()) return;
    uint8_t data[c4::RECORD_BYTES];
    c4::packRecord(record_, data);
    recordFailed_ = !storage::save(RECORD_FILE, c4::RECORD_VERSION, data, sizeof(data));
  }

  void loadRecord() {
    if (!storage::ready()) return;
    uint8_t data[c4::RECORD_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(RECORD_FILE, version, data, sizeof(data), size)) return;
    if (version != c4::RECORD_VERSION || !c4::unpackRecord(data, size, record_)) {
      Serial.println("Connect Four: the record didn't check out, so it starts again");
    }
  }

  c4::Game game_{};
  c4::Screen screen_ = c4::Screen::Menu;
  bool loaded_ = false;
  bool started_ = false;   // game_ is a game the player began, not the empty default
  bool recorded_ = false;  // this game's ending is in the record
  int howToPage_ = 0;
  c4::Record record_;
  bool gameFailed_ = false;
  bool recordFailed_ = false;
};

ConnectFour instance;

}  // namespace

Game& connectFourGame() { return instance; }

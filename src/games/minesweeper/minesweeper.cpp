// Minesweeper, ported from CrossPlay's MinesweeperActivity.cpp. Tap to dig, hold
// to plant a flag, or switch what a tap does with DIG/FLAG under the board.
#include "minesweeper.h"

#include <Arduino.h>
#include <esp_random.h>

#include "core/storage.h"
#include "minesweeper_flow.h"
#include "minesweeper_icons.h"
#include "minesweeper_rules.h"
#include "minesweeper_save.h"
#include "minesweeper_screens.h"
#include "ui/ui.h"

namespace {

namespace ms = minesweeper;

constexpr const char* BOARD_FILE = "minesweeper";
constexpr const char* HISTORY_FILE = "minesweeper-history";
constexpr int MAX_TALLY = 0xFFFF;  // the history keeps each count in two bytes

class Minesweeper final : public Game {
 public:
  const char* name() const override { return "MINESWEEPER"; }
  const uint8_t* icon() const override { return minesweeperIcons::HOME; }
  bool usesHold() const override { return true; }

  // The menu every time, as in CrossPlay, with a board in play offered as RESUME.
  void open() override {
    if (!loaded_) {
      loadBoard();
      loadHistory();
      loaded_ = true;
    }
    screen_ = ms::Screen::Menu;
    clearHold();
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) {
      if (ms::leavesApp(screen_)) return TapResult::Leave;
      screen_ = ms::back(screen_);
      return TapResult::Redraw;
    }
    switch (screen_) {
      case ms::Screen::Menu:
        return tapMenu(x, y);
      case ms::Screen::HowTo:
        return tapHowTo(x, y);
      case ms::Screen::Board:
        return tapBoard(x, y);
      case ms::Screen::Result:
        return tapResult(x, y);
    }
    return TapResult::Nothing;
  }

  // The cell under a finger is marked while a hold builds: on this panel an
  // unmarked hold looks like a tap that missed.
  TapResult onPress(int x, int y) override {
    int column = 0;
    int row = 0;
    if (screen_ != ms::Screen::Board || ms::over(game_) || !mineui::cellAt(x, y, column, row)) return TapResult::Nothing;
    holdColumn_ = column;
    holdRow_ = row;
    return TapResult::Redraw;
  }

  // Still a flag in FLAG mode too, never a dig: a hold nobody meant should cost at
  // most a tap to undo, and a dig can end the game.
  TapResult onHold(int x, int y) override {
    int column = 0;
    int row = 0;
    if (screen_ != ms::Screen::Board || !mineui::cellAt(x, y, column, row)) return TapResult::Nothing;
    if (!ms::toggleFlag(game_, column, row)) return TapResult::Nothing;
    persist();
    return TapResult::Redraw;
  }

  TapResult onRelease() override {
    if (holdColumn_ < 0) return TapResult::Nothing;
    clearHold();
    return TapResult::Redraw;
  }

  void draw(uint8_t* frame) override {
    switch (screen_) {
      case ms::Screen::Menu: {
        mineui::MenuModel model;
        model.inProgress = inProgress();
        model.hasHistory = hasHistory_;
        model.lastBoard = &history_.lastBoard;
        model.wins = history_.wins;
        model.losses = history_.losses;
        mineui::drawMenu(frame, model);
        break;
      }
      case ms::Screen::HowTo:
        mineui::drawHowTo(frame, howToPage_);
        break;
      case ms::Screen::Board: {
        mineui::BoardModel model;
        model.game = &game_;
        model.holdColumn = holdColumn_;
        model.holdRow = holdRow_;
        model.flagMode = flagMode_;
        mineui::drawBoard(frame, model);
        break;
      }
      case ms::Screen::Result:
        mineui::drawResult(frame, resultModel());
        break;
    }
  }

  bool saveFailed() const override { return boardFailed_ || historyFailed_; }

  void retrySave() override {
    if (boardFailed_) persist();
    if (historyFailed_) saveHistory();
  }

 private:
  bool inProgress() const { return started_ && !ms::over(game_); }

  void clearHold() {
    holdColumn_ = -1;
    holdRow_ = -1;
  }

  TapResult tapMenu(int x, int y) {
    mineui::MenuModel model;
    model.inProgress = inProgress();
    switch (mineui::menuTapAt(model, x, y)) {
      case mineui::MenuTap::Nothing:
        return TapResult::Nothing;
      case mineui::MenuTap::Resume:
        clearHold();
        screen_ = ms::Screen::Board;
        return TapResult::RedrawFull;
      case mineui::MenuTap::Play:
      case mineui::MenuTap::NewGame:
        return beginGame();
      case mineui::MenuTap::HowTo:
        howToPage_ = 0;
        screen_ = ms::Screen::HowTo;
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult tapHowTo(int x, int y) {
    if (!mineui::isOnHowToNext(x, y)) return TapResult::Nothing;
    if (howToPage_ + 1 < mineui::howToPages()) {
      ++howToPage_;
    } else {
      screen_ = ms::Screen::Menu;
    }
    return TapResult::Redraw;
  }

  TapResult tapBoard(int x, int y) {
    int column = 0;
    int row = 0;
    if (mineui::cellAt(x, y, column, row)) {
      // FLAG mode doesn't chord: chording is a dig, and the FLAG tool shouldn't be
      // able to end the game. ms::dig() opens a covered cell or chords a number.
      const bool changed = flagMode_ ? ms::toggleFlag(game_, column, row) : ms::dig(game_, column, row);
      if (!changed) return TapResult::Nothing;
      if (ms::over(game_)) return settle();
      persist();
      return TapResult::Redraw;
    }
    if (ms::over(game_)) {
      if (!mineui::isOnVerdict(x, y)) return TapResult::Nothing;
      screen_ = ms::Screen::Result;
      return TapResult::Redraw;
    }
    if (mineui::isOnMode(x, y)) {
      flagMode_ = !flagMode_;
      persist();
      return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult tapResult(int x, int y) {
    switch (mineui::resultTapAt(x, y)) {
      case mineui::ResultTap::Nothing:
        return TapResult::Nothing;
      case mineui::ResultTap::Again:
        return beginGame();
      case mineui::ResultTap::Done:
        screen_ = ms::Screen::Menu;
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult beginGame() {
    // The chip's hardware random numbers. The rules take a seed and never reach
    // for a clock, as in CrossPlay.
    ms::start(game_, esp_random());
    started_ = true;
    flagMode_ = false;
    clearHold();
    screen_ = ms::Screen::Board;
    persist();
    return TapResult::RedrawFull;
  }

  // The board stays on screen, mines bared, with its verdict under it. The record
  // is written now, and the save goes, so the menu doesn't offer it again.
  TapResult settle() {
    history_.lastBoard = game_;
    if (game_.status == ms::Status::Won && history_.wins < MAX_TALLY) ++history_.wins;
    if (game_.status == ms::Status::Lost && history_.losses < MAX_TALLY) ++history_.losses;
    hasHistory_ = true;
    clearHold();
    persist();
    saveHistory();
    return TapResult::RedrawFull;
  }

  mineui::ResultModel resultModel() const {
    mineui::ResultModel model;
    model.won = game_.status == ms::Status::Won;
    for (int column = 0; column < ms::kColumns; ++column) {
      for (int row = 0; row < ms::kRows; ++row) {
        const uint8_t cell = game_.cell[column][row];
        if ((cell & ms::kRevealed) && !(cell & ms::kMine)) ++model.revealed;
        if ((cell & ms::kFlagged) && (cell & ms::kMine)) ++model.flagsRight;
      }
    }
    return model;
  }

  // --- Saving ---

  void persist() {
    if (!storage::ready()) return;
    if (!inProgress()) {
      boardFailed_ = !storage::erase(BOARD_FILE);
      return;
    }
    ms::OpenBoard board;
    board.game = game_;
    board.flagMode = flagMode_;
    uint8_t data[ms::BOARD_BYTES];
    ms::packBoard(board, data);
    boardFailed_ = !storage::save(BOARD_FILE, ms::BOARD_VERSION, data, sizeof(data));
  }

  void loadBoard() {
    if (!storage::ready()) return;
    uint8_t data[ms::BOARD_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(BOARD_FILE, version, data, sizeof(data), size)) return;
    ms::OpenBoard board;
    if (version != ms::BOARD_VERSION || !ms::unpackBoard(data, size, board)) {
      Serial.println("Minesweeper: the board didn't check out, so it's set aside");
      return;
    }
    game_ = board.game;
    flagMode_ = board.flagMode;
    started_ = true;
  }

  void saveHistory() {
    if (!storage::ready()) return;
    uint8_t data[ms::HISTORY_BYTES];
    ms::packHistory(history_, data);
    historyFailed_ = !storage::save(HISTORY_FILE, ms::HISTORY_VERSION, data, sizeof(data));
  }

  void loadHistory() {
    if (!storage::ready()) return;
    uint8_t data[ms::HISTORY_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(HISTORY_FILE, version, data, sizeof(data), size)) return;
    if (version != ms::HISTORY_VERSION || !ms::unpackHistory(data, size, history_)) {
      Serial.println("Minesweeper: the record didn't check out, so it starts again");
      return;
    }
    hasHistory_ = true;
  }

  ms::Game game_{};
  ms::Screen screen_ = ms::Screen::Menu;
  bool loaded_ = false;
  bool started_ = false;  // game_ is a board the player began, not the empty default
  bool flagMode_ = false;
  int holdColumn_ = -1;
  int holdRow_ = -1;
  int howToPage_ = 0;
  ms::History history_;
  bool hasHistory_ = false;
  bool boardFailed_ = false;
  bool historyFailed_ = false;
};

Minesweeper instance;

}  // namespace

Game& minesweeperGame() { return instance; }

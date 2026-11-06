// Sudoku, ported from CrossPlay's SudokuActivity.cpp. Puzzles are made on the
// device and graded by the hardest technique they need. Tap to write the armed
// digit, hold to pencil it.
#include "sudoku.h"

#include <Arduino.h>
#include <esp_random.h>

#include <cstring>

#include "core/storage.h"
#include "sudoku_flow.h"
#include "sudoku_game.h"
#include "sudoku_icon.h"
#include "sudoku_rules.h"
#include "sudoku_save.h"
#include "sudoku_screens.h"
#include "ui/ui.h"

namespace {

namespace sk = sudoku;

constexpr const char* GAME_FILE = "sudoku";
constexpr const char* RECORD_FILE = "sudoku-record";
// Attempts per loop pass while a puzzle is being made, as CrossPlay's: the loop is
// never held for the whole carve.
constexpr int ATTEMPTS_PER_PASS = 24;

// Whether a move left the player's side of the game as it was. Field by field,
// because comparing the struct's bytes would count its padding.
bool sameState(const sk::Game& a, const sk::Game& b) {
  if (memcmp(a.entry, b.entry, sizeof(a.entry)) != 0 || memcmp(a.note, b.note, sizeof(a.note)) != 0) return false;
  return a.armed == b.armed && a.hintCell == b.hintCell && a.hintDigit == b.hintDigit && a.hintTechnique == b.hintTechnique &&
         a.hintsUsed == b.hintsUsed && a.solvedFlag == b.solvedFlag && a.undoCount == b.undoCount && a.undoNext == b.undoNext;
}

sk::Level nextLevel(sk::Level level) { return static_cast<sk::Level>((static_cast<int>(level) + 1) % sk::kLevelCount); }

class Sudoku final : public Game {
 public:
  const char* name() const override { return "SUDOKU"; }
  const uint8_t* icon() const override { return sudokuIcon::ICON; }
  bool usesHold() const override { return true; }

  // The menu every time, as in CrossPlay, with the puzzle in play as RESUME.
  void open() override {
    if (!loaded_) {
      rng_ = esp_random();
      loadState();
      loadRecord();
      loaded_ = true;
    }
    screen_ = sk::Screen::Menu;
    levelSelected_ = false;
    notice_ = nullptr;
    holdCell_ = sk::kNoCell;
    lastTickMs_ = millis();
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) {
      if (sk::leavesApp(screen_)) return TapResult::Leave;
      if (screen_ == sk::Screen::Board) {
        generating_ = false;
        persist();
      }
      goTo(sk::back(screen_));
      return TapResult::Redraw;
    }
    switch (screen_) {
      case sk::Screen::Menu:
        return tapMenu(x, y);
      case sk::Screen::HowTo:
        return tapHowTo(x, y);
      case sk::Screen::Board:
        return tapBoard(x, y);
      case sk::Screen::Result:
        return tapResult(x, y);
    }
    return TapResult::Nothing;
  }

  // The cell under a finger turns dark while a hold builds.
  TapResult onPress(int x, int y) override {
    int cell = 0;
    if (screen_ != sk::Screen::Board || generating_ || !sudokuui::cellAt(x, y, cell)) return TapResult::Nothing;
    holdCell_ = cell;
    return TapResult::Redraw;
  }

  TapResult onHold(int x, int y) override {
    int cell = 0;
    if (screen_ != sk::Screen::Board || generating_ || !sudokuui::cellAt(x, y, cell)) return TapResult::Nothing;
    return change([&] {
      notice_ = nullptr;
      sk::holdCell(game_, cell);
    });
  }

  TapResult onRelease() override {
    if (holdCell_ == sk::kNoCell) return TapResult::Nothing;
    holdCell_ = sk::kNoCell;
    return TapResult::Redraw;
  }

  // Makes the puzzle a pass at a time, and keeps the clock while the board is open.
  bool update() override {
    const unsigned long now = millis();
    const unsigned long since = now - lastTickMs_;
    lastTickMs_ = now;
    if (generating_) {
      sk::Puzzle puzzle;
      if (!sk::generate(puzzle, generatingLevel_, work_, rng_, ATTEMPTS_PER_PASS)) return false;
      sk::startGame(game_, puzzle);
      hasGame_ = true;
      generating_ = false;
      persist();
      return true;
    }
    if (screen_ == sk::Screen::Board && hasGame_ && game_.solvedFlag == 0) game_.elapsedMs += static_cast<uint32_t>(since);
    return false;
  }

  bool busy() const override { return generating_; }

  void draw(uint8_t* frame) override {
    switch (screen_) {
      case sk::Screen::Menu: {
        sudokuui::MenuModel model;
        model.level = menuLevel_;
        model.hasGame = hasGame_;
        model.game = &game_;
        model.record = &record_;
        model.levelSelected = levelSelected_;
        sudokuui::drawMenu(frame, model);
        break;
      }
      case sk::Screen::HowTo:
        sudokuui::drawHowTo(frame, howToPage_);
        break;
      case sk::Screen::Board: {
        sudokuui::BoardModel model;
        model.game = &game_;
        model.holdCell = holdCell_;
        model.generating = generating_;
        model.notice = notice_;
        sudokuui::drawBoard(frame, model);
        break;
      }
      case sk::Screen::Result: {
        sudokuui::ResultModel model;
        model.game = &game_;
        model.bestMs = record_.bestMs[static_cast<int>(game_.puzzle.level)];
        model.newBest = newBest_;
        model.solvedAtThisLevel = record_.solved[static_cast<int>(game_.puzzle.level)];
        sudokuui::drawResult(frame, model);
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
  void goTo(sk::Screen next) {
    screen_ = next;
    notice_ = nullptr;
    holdCell_ = sk::kNoCell;
  }

  // Runs a move, and says whether it changed anything: a tap that changes nothing
  // draws nothing. A move that solves the grid records it and gets the full refresh.
  template <typename Move>
  TapResult change(Move move) {
    const sk::Game before = game_;
    const char* noticeBefore = notice_;
    move();
    const bool changed = !sameState(before, game_) || notice_ != noticeBefore;
    if (!changed) return TapResult::Nothing;
    if (game_.solvedFlag != 0 && before.solvedFlag == 0) {
      recordResult();
      persist();
      return TapResult::RedrawFull;
    }
    persist();
    return TapResult::Redraw;
  }

  TapResult tapMenu(int x, int y) {
    switch (sudokuui::menuTapAt(x, y)) {
      case sudokuui::MenuTap::Nothing:
        return TapResult::Nothing;
      case sudokuui::MenuTap::Play:
        levelSelected_ = false;
        if (sk::canResume(game_, hasGame_, menuLevel_)) {
          goTo(sk::Screen::Board);
          return TapResult::RedrawFull;
        }
        return beginGame();
      case sudokuui::MenuTap::Level:
        menuLevel_ = nextLevel(menuLevel_);
        levelSelected_ = true;
        persist();
        return TapResult::Redraw;
      case sudokuui::MenuTap::HowTo:
        levelSelected_ = false;
        howToPage_ = 0;
        goTo(sk::Screen::HowTo);
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult tapHowTo(int x, int y) {
    if (!sudokuui::isOnHowToNext(x, y)) return TapResult::Nothing;
    if (howToPage_ + 1 < sudokuui::howToPages()) {
      ++howToPage_;
    } else {
      goTo(sk::Screen::Menu);
    }
    return TapResult::Redraw;
  }

  TapResult tapBoard(int x, int y) {
    if (generating_) return TapResult::Nothing;
    int cell = 0;
    int digit = 0;
    if (sudokuui::cellAt(x, y, cell)) {
      return change([&] {
        notice_ = nullptr;
        sk::tapCell(game_, cell);
      });
    }
    if (sudokuui::padKeyAt(x, y, digit)) {
      return change([&] {
        notice_ = nullptr;
        game_.armed = static_cast<uint8_t>(digit);
      });
    }
    switch (sudokuui::railTapAt(x, y)) {
      case sudokuui::RailTap::Nothing:
        return TapResult::Nothing;
      case sudokuui::RailTap::Status:
        // The capsule is the door to the result once the grid is solved, and
        // nothing before that.
        if (game_.solvedFlag == 0) return TapResult::Nothing;
        goTo(sk::Screen::Result);
        return TapResult::Redraw;
      case sudokuui::RailTap::Undo:
        return change([&] {
          if (sk::undoOnce(game_)) notice_ = nullptr;
        });
      case sudokuui::RailTap::Hint:
        if (game_.solvedFlag != 0) return TapResult::Nothing;
        return change([&] { takeHint(); });
    }
    return TapResult::Nothing;
  }

  TapResult tapResult(int x, int y) {
    switch (sudokuui::resultTapAt(x, y)) {
      case sudokuui::ResultTap::Nothing:
        return TapResult::Nothing;
      case sudokuui::ResultTap::Another:
        return beginGame();
      case sudokuui::ResultTap::Grid:
        goTo(sk::Screen::Board);
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  // The board opens at once saying MAKING ONE, and update() carves the puzzle.
  TapResult beginGame() {
    generating_ = true;
    generatingLevel_ = menuLevel_;
    newBest_ = false;
    goTo(sk::Screen::Board);
    return TapResult::RedrawFull;
  }

  // HINT names a cell and the rule, never the digit, and checks first for a digit
  // that disagrees with the answer. CrossPlay's.
  void takeHint() {
    const int wrong = sk::firstWrong(game_);
    if (wrong != sk::kNoCell) {
      game_.hintCell = static_cast<uint8_t>(wrong);
      game_.hintDigit = 0;
      notice_ = "WRONG DIGIT";
      ++game_.hintsUsed;
      return;
    }
    uint8_t board[sk::kCells];
    for (int cell = 0; cell < sk::kCells; ++cell) board[cell] = sk::valueAt(game_, cell);
    const sk::Hint hint = sk::nextHint(board, sk::ceilingFor(sk::Level::Expert));
    if (!hint.found) {
      notice_ = "NOTHING YET";
      return;
    }
    game_.hintCell = static_cast<uint8_t>(hint.cell);
    game_.hintDigit = static_cast<uint8_t>(hint.digit);
    game_.hintTechnique = static_cast<uint8_t>(hint.technique);
    ++game_.hintsUsed;
    notice_ = sk::techniqueName(hint.technique);
  }

  void recordResult() {
    const int index = static_cast<int>(game_.puzzle.level);
    const uint32_t previousBest = record_.bestMs[index];
    sk::recordSolve(record_, game_.puzzle.level, game_.elapsedMs, game_.hintsUsed);
    newBest_ = game_.hintsUsed == 0 && record_.bestMs[index] == game_.elapsedMs && previousBest != game_.elapsedMs;
    saveRecord();
  }

  // --- Saving ---

  // A solved puzzle stays saved, as in CrossPlay, because the menu draws it and the
  // result returns to it. It's never offered as RESUME: sk::menuOffer says Solved.
  void persist() {
    if (!storage::ready()) return;
    sk::Saved saved;
    saved.game = game_;
    saved.hasGame = hasGame_;
    saved.menuLevel = menuLevel_;
    uint8_t data[sk::SAVE_BYTES];
    sk::packGame(saved, data);
    gameFailed_ = !storage::save(GAME_FILE, sk::SAVE_VERSION, data, sizeof(data));
  }

  void loadState() {
    if (!storage::ready()) return;
    uint8_t data[sk::SAVE_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(GAME_FILE, version, data, sizeof(data), size)) return;
    sk::Saved saved;
    if (version != sk::SAVE_VERSION || !sk::unpackGame(data, size, saved)) {
      Serial.println("Sudoku: the save didn't check out, so it's set aside");
      return;
    }
    game_ = saved.game;
    hasGame_ = saved.hasGame;
    menuLevel_ = saved.menuLevel;
    // A level browsed to and never played isn't worth keeping over a puzzle in play.
    if (hasGame_ && game_.solvedFlag == 0) menuLevel_ = game_.puzzle.level;
  }

  void saveRecord() {
    if (!storage::ready()) return;
    uint8_t data[sk::RECORD_BYTES];
    sk::packRecord(record_, data);
    recordFailed_ = !storage::save(RECORD_FILE, sk::RECORD_VERSION, data, sizeof(data));
  }

  void loadRecord() {
    if (!storage::ready()) return;
    uint8_t data[sk::RECORD_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(RECORD_FILE, version, data, sizeof(data), size)) return;
    if (version != sk::RECORD_VERSION || !sk::unpackRecord(data, size, record_)) {
      Serial.println("Sudoku: the record didn't check out, so it starts again");
      record_ = sk::Record{};
    }
  }

  sk::Game game_{};
  sk::Record record_{};
  sk::Workspace work_{};
  sk::Screen screen_ = sk::Screen::Menu;
  sk::Level menuLevel_ = sk::Level::Easy;
  sk::Level generatingLevel_ = sk::Level::Easy;
  uint32_t rng_ = 0;
  bool loaded_ = false;
  bool hasGame_ = false;
  bool generating_ = false;
  bool levelSelected_ = false;
  bool newBest_ = false;
  int holdCell_ = sk::kNoCell;
  unsigned long lastTickMs_ = 0;
  const char* notice_ = nullptr;
  int howToPage_ = 0;
  bool gameFailed_ = false;
  bool recordFailed_ = false;
};

Sudoku instance;

}  // namespace

Game& sudokuGame() { return instance; }

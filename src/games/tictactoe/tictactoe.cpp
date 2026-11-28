#include "tictactoe.h"

#include <Arduino.h>
#include <esp_random.h>

#include <initializer_list>

#include "core/graphics.h"
#include "core/storage.h"
#include "core/text.h"
#include "tictactoe_brain.h"
#include "tictactoe_flow.h"
#include "tictactoe_icon.h"
#include "tictactoe_rules.h"
#include "tictactoe_save.h"
#include "ui/ui.h"

namespace ttt = tictactoe;

namespace {

constexpr const char* SAVE_FILE = "tictactoe";

// CrossPlay's layout: the status in a capsule under the header, the board, the rules,
// and NEW GAME along the bottom.
constexpr int MARGIN = 16;
constexpr int INNER_WIDTH = SCREEN_WIDTH - 2 * MARGIN;
constexpr int STATUS_TOP = ui::CONTENT_TOP;
constexpr int STATUS_HEIGHT = ui::PILL_HEIGHT;
constexpr int STATUS_BORDER = 2;
constexpr int BOARD_LEFT = MARGIN;
constexpr int BOARD_TOP = STATUS_TOP + STATUS_HEIGHT + 12;
constexpr int CELL_SIZE = INNER_WIDTH / ttt::kSize;
constexpr int BOARD_SIZE = CELL_SIZE * ttt::kSize;
constexpr int GRID_WEIGHT = 4;
constexpr int MARK_WEIGHT = CELL_SIZE / 11;
constexpr int O_RADIUS = CELL_SIZE * 3 / 10;
constexpr int X_REACH = O_RADIUS * 4 / 5;

constexpr int BUTTON_HEIGHT = ui::PILL_HEIGHT;
constexpr int BUTTON_TOP = SCREEN_HEIGHT - MARGIN - BUTTON_HEIGHT;

// The mode buttons sit in one row above NEW GAME, all the same width and the same
// cut, with the mode being played filled.
constexpr int MODE_GAP = 8;
constexpr int MODE_RADIUS = 14;
constexpr int MODE_TOP = BUTTON_TOP - ui::GAP - BUTTON_HEIGHT;

enum class ModeButton : uint8_t { TwoPlayers, Easy, Hard, Nearby };
// Two-device play isn't here yet, so NEARBY is drawn greyed and does nothing.
constexpr bool NEARBY_READY = false;
constexpr const char* MODE_LABELS[] = {"2P", "EASY", "HARD", "NEARBY"};
constexpr int MODE_COUNT = 4;
constexpr int MODE_WIDTH = (INNER_WIDTH - (MODE_COUNT - 1) * MODE_GAP) / MODE_COUNT;

// While a new game waits to be confirmed, the bottom row splits in two.
constexpr int HALF_WIDTH = (INNER_WIDTH - ui::GAP) / 2;
constexpr int CONFIRM_LEFT = MARGIN + HALF_WIDTH + ui::GAP;

int modeLeft(int i) { return MARGIN + i * (MODE_WIDTH + MODE_GAP); }

// The largest cut every label fits, so the row reads as one control.
Font modeFont() {
  for (const Font font : {Font::Size20, Font::Size14}) {
    bool fits = true;
    for (const char* label : MODE_LABELS) fits = fits && textWidth(font, label) <= MODE_WIDTH - 12;
    if (fits) return font;
  }
  return Font::Size10;
}

int modeAt(int x, int y) {
  for (int i = 0; i < MODE_COUNT; i++) {
    if (ui::isInBox(x, y, modeLeft(i), MODE_TOP, MODE_WIDTH, BUTTON_HEIGHT)) return i;
  }
  return -1;
}

void drawModes(uint8_t* frame, int selected) {
  const Font font = modeFont();
  for (int i = 0; i < MODE_COUNT; i++) {
    const bool on = i == selected;
    if (i == static_cast<int>(ModeButton::Nearby) && !NEARBY_READY) {
      // Greyed, not hidden, so the row doesn't move when it arrives: one dot in four,
      // inside the corners.
      for (int y = MODE_TOP + MODE_RADIUS / 2; y < MODE_TOP + BUTTON_HEIGHT - MODE_RADIUS / 2; y++) {
        for (int x = modeLeft(i) + MODE_RADIUS / 2; x < modeLeft(i) + MODE_WIDTH - MODE_RADIUS / 2; x++) {
          if (x % 2 == 0 && y % 2 == 0) setPixel(frame, x, y, true);
        }
      }
    }
    if (on) {
      fillRoundRect(frame, modeLeft(i), MODE_TOP, MODE_WIDTH, BUTTON_HEIGHT, MODE_RADIUS, true);
    } else {
      drawRoundRectOutline(frame, modeLeft(i), MODE_TOP, MODE_WIDTH, BUTTON_HEIGHT, MODE_RADIUS, ui::RULE);
    }
    drawTextInBox(frame, font, modeLeft(i), MODE_TOP, MODE_WIDTH, BUTTON_HEIGHT, MODE_LABELS[i], !on);
  }
}

// There's no line drawing, so each stroke is a square brush stepped along the
// diagonal, which keeps it as heavy as a straight line would be.
void drawX(uint8_t* frame, int centerX, int centerY, bool black) {
  for (int step = -X_REACH; step <= X_REACH; step++) {
    const int left = centerX + step - MARK_WEIGHT / 2;
    fillRect(frame, left, centerY + step - MARK_WEIGHT / 2, MARK_WEIGHT, MARK_WEIGHT, black);
    fillRect(frame, left, centerY - step - MARK_WEIGHT / 2, MARK_WEIGHT, MARK_WEIGHT, black);
  }
}

void drawGrid(uint8_t* frame) {
  for (int i = 1; i < ttt::kSize; i++) {
    const int offset = i * CELL_SIZE - GRID_WEIGHT / 2;
    fillRect(frame, BOARD_LEFT + offset, BOARD_TOP, GRID_WEIGHT, BOARD_SIZE, true);
    fillRect(frame, BOARD_LEFT, BOARD_TOP + offset, BOARD_SIZE, GRID_WEIGHT, true);
  }
}

// The winning three are inverted, not crossed out: a line through three X marks on a
// diagonal runs along one stroke of each and can't be seen.
void drawCell(uint8_t* frame, int cell, uint8_t mark, bool won) {
  const int left = BOARD_LEFT + (cell % ttt::kSize) * CELL_SIZE;
  const int top = BOARD_TOP + (cell / ttt::kSize) * CELL_SIZE;
  if (won) fillRect(frame, left + GRID_WEIGHT, top + GRID_WEIGHT, CELL_SIZE - 2 * GRID_WEIGHT, CELL_SIZE - 2 * GRID_WEIGHT, true);

  const int centerX = left + CELL_SIZE / 2;
  const int centerY = top + CELL_SIZE / 2;
  if (mark == ttt::kX) {
    drawX(frame, centerX, centerY, !won);
  } else if (mark == ttt::kO) {
    drawRing(frame, centerX, centerY, O_RADIUS, MARK_WEIGHT, !won);
  }
}

bool boardIsEmpty(const ttt::Game& game) {
  for (const uint8_t cell : game.cell) {
    if (cell != ttt::kEmpty) return false;
  }
  return true;
}

ttt::Level levelOf(ttt::Mode mode) { return mode == ttt::Mode::Hard ? ttt::Level::Hard : ttt::Level::Easy; }

class TicTacToe final : public Game {
 public:
  const char* name() const override { return "TIC TAC TOE"; }
  const uint8_t* icon() const override { return tictactoeIcon::ICON; }

  // Memory is the truth once loaded, so a game also survives leaving and coming back
  // without a card. A finished game doesn't: coming back starts a new one, with the
  // same players.
  void open() override {
    if (!loaded_) {
      loaded_ = true;
      ttt::start(saved_.game);
      saved_.mode = ttt::Mode::TwoPlayers;
      saved_.computer = ttt::kEmpty;
      load();
    } else if (ttt::over(saved_.game)) {
      startGame(saved_.mode);
    }
    confirming_ = false;
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) return TapResult::Leave;
    if (confirming_) return tapConfirm(x, y);

    if (ui::isInBox(x, y, MARGIN, BUTTON_TOP, INNER_WIDTH, BUTTON_HEIGHT)) {
      // With nothing to clear there's nothing to start: a computer that opens has
      // already moved, so its new board is never empty.
      if (boardIsEmpty(saved_.game)) return TapResult::Nothing;
      return askToStart(saved_.mode);
    }

    const int mode = modeAt(x, y);
    if (mode >= 0) return tapMode(static_cast<ModeButton>(mode));

    if (computersTurn()) return TapResult::Nothing;
    if (!ui::isInBox(x, y, BOARD_LEFT, BOARD_TOP, BOARD_SIZE, BOARD_SIZE)) return TapResult::Nothing;
    const int column = (x - BOARD_LEFT) / CELL_SIZE;
    const int row = (y - BOARD_TOP) / CELL_SIZE;
    if (!ttt::place(saved_.game, row * ttt::kSize + column)) return TapResult::Nothing;

    persist();
    return ttt::over(saved_.game) ? TapResult::RedrawFull : TapResult::Redraw;
  }

  // The computer's mark goes down on the pass after yours, so your mark is drawn
  // before it answers. An opening move is made when the game starts instead, so a
  // new game doesn't refresh twice.
  bool update() override {
    if (!computersTurn() || confirming_) return false;
    const int cell = ttt::chooseCell(saved_.game, levelOf(saved_.mode), rng_);
    if (!ttt::place(saved_.game, cell)) return false;
    persist();
    return true;
  }

  void draw(uint8_t* frame) override { drawBoard(frame); }

  bool saveFailed() const override { return saveFailed_; }

  void retrySave() override { persist(); }

 private:
  bool vsComputer() const { return saved_.mode != ttt::Mode::TwoPlayers; }
  bool computersTurn() const { return vsComputer() && !ttt::over(saved_.game) && saved_.game.turn == saved_.computer; }
  bool inProgress() const { return !ttt::over(saved_.game) && !boardIsEmpty(saved_.game); }

  // A mode button starts a new game with those players. The one being played does
  // what NEW GAME does.
  TapResult tapMode(ModeButton button) {
    ttt::Mode mode = ttt::Mode::TwoPlayers;
    switch (button) {
      case ModeButton::TwoPlayers:
        break;
      case ModeButton::Easy:
        mode = ttt::Mode::Easy;
        break;
      case ModeButton::Hard:
        mode = ttt::Mode::Hard;
        break;
      case ModeButton::Nearby:
        return TapResult::Nothing;  // two-device play comes next
    }
    if (mode == saved_.mode && boardIsEmpty(saved_.game)) return TapResult::Nothing;
    return askToStart(mode);
  }

  // A stray tap on a mode or NEW GAME mustn't throw a game away, so a game with a
  // move of yours in it asks first. Any other game starts at once.
  TapResult askToStart(ttt::Mode mode) {
    if (!worthKeeping()) {
      startGame(mode);
      return TapResult::RedrawFull;
    }
    confirming_ = true;
    pendingMode_ = mode;
    return TapResult::Redraw;
  }

  // NEW GAME starts the mode that was tapped. Anything else, KEEP PLAYING or not,
  // goes back to the game: a tap on the board is read as changing your mind, not as
  // a move, because you couldn't see the board was live.
  TapResult tapConfirm(int x, int y) {
    confirming_ = false;
    if (ui::isInBox(x, y, CONFIRM_LEFT, BUTTON_TOP, HALF_WIDTH, BUTTON_HEIGHT)) {
      startGame(pendingMode_);
      return TapResult::RedrawFull;
    }
    return TapResult::Redraw;
  }

  bool worthKeeping() const { return ttt::worthKeeping(saved_.game, saved_.computer); }

  // Against the computer the opener alternates, game by game: you first, then it.
  void startGame(ttt::Mode mode) {
    ttt::start(saved_.game);
    saved_.mode = mode;
    saved_.computer = ttt::kEmpty;
    if (mode != ttt::Mode::TwoPlayers) {
      saved_.computer = computerOpensNext_ ? ttt::kX : ttt::kO;
      computerOpensNext_ = !computerOpensNext_;
      seedRng();
      if (computersTurn()) ttt::place(saved_.game, ttt::chooseCell(saved_.game, levelOf(mode), rng_));
    }
    persist();
  }

  // The chip's random number, stirred with the microsecond of the tap that started
  // the game. xorshift must never hold 0.
  void seedRng() {
    rng_.state = esp_random() ^ static_cast<uint32_t>(micros());
    if (rng_.state == 0) rng_.state = 0x9E3779B9u;
  }

  const char* statusText() const {
    if (confirming_) return "END THIS GAME?";
    const ttt::Game& game = saved_.game;
    if (!vsComputer()) {
      switch (game.outcome) {
        case ttt::Outcome::Running:
          return game.turn == ttt::kX ? "X TO MOVE" : "O TO MOVE";
        case ttt::Outcome::XWins:
          return "X WINS";
        case ttt::Outcome::OWins:
          return "O WINS";
        case ttt::Outcome::Draw:
          return "A DRAW";
      }
      return "";
    }
    switch (game.outcome) {
      case ttt::Outcome::Running:
        if (game.turn == saved_.computer) return "THINKING";
        return saved_.computer == ttt::kX ? "YOUR MOVE (O)" : "YOUR MOVE (X)";
      case ttt::Outcome::Draw:
        return "A DRAW";
      default:
        return game.turn == saved_.computer ? "COMPUTER WINS" : "YOU WIN";
    }
  }

  void drawBoard(uint8_t* frame) {
    const ttt::Game& game = saved_.game;
    ui::drawBand(frame, name(), nullptr, true);

    // Outlined while the game runs, and solid once it's over: a fill that appears
    // once, with the full refresh, is the payoff rather than a cost.
    const bool over = ttt::over(game);
    if (over) {
      fillRect(frame, MARGIN, STATUS_TOP, INNER_WIDTH, STATUS_HEIGHT, true);
    } else {
      drawRectOutline(frame, MARGIN, STATUS_TOP, INNER_WIDTH, STATUS_HEIGHT, STATUS_BORDER);
    }
    drawTextInBox(frame, Font::Size30, MARGIN, STATUS_TOP, INNER_WIDTH, STATUS_HEIGHT, statusText(), !over);

    int line[ttt::kLine] = {-1, -1, -1};
    if (game.outcome == ttt::Outcome::XWins) ttt::winningLine(game, ttt::kX, line);
    if (game.outcome == ttt::Outcome::OWins) ttt::winningLine(game, ttt::kO, line);

    drawGrid(frame);
    for (int cell = 0; cell < ttt::kCells; cell++) {
      const bool won = cell == line[0] || cell == line[1] || cell == line[2];
      drawCell(frame, cell, game.cell[cell], won);
    }

    // While confirming, the mode that would start is the one filled.
    drawModes(frame, static_cast<int>(confirming_ ? pendingMode_ : saved_.mode));

    if (confirming_) {
      ui::drawPill(frame, MARGIN, BUTTON_TOP, HALF_WIDTH, "KEEP PLAYING", false);
      ui::drawPill(frame, CONFIRM_LEFT, BUTTON_TOP, HALF_WIDTH, "NEW GAME", true);
    } else {
      ui::drawPill(frame, MARGIN, BUTTON_TOP, INNER_WIDTH, "NEW GAME", true);
    }
  }

  void load() {
    if (!storage::ready()) return;

    uint8_t data[ttt::SAVE_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(SAVE_FILE, version, data, sizeof(data), size)) return;
    if (!ttt::unpackGame(version, data, size, saved_)) {
      Serial.println("Ignoring saved Tic Tac Toe game");
      return;
    }
    // You opened this one, so the computer opens the next, and the other way round.
    computerOpensNext_ = saved_.computer == ttt::kO;
  }

  // Only a game in progress has a save. Otherwise it would be offered for resume
  // after it has ended.
  void persist() {
    if (!storage::ready()) {
      saveFailed_ = false;
      return;
    }

    if (!inProgress()) {
      saveFailed_ = !storage::erase(SAVE_FILE);
      return;
    }

    uint8_t data[ttt::SAVE_BYTES];
    const size_t size = ttt::packGame(saved_, data);
    const uint32_t startUs = micros();
    saveFailed_ = !storage::save(SAVE_FILE, ttt::SAVE_VERSION, data, size);
    Serial.printf("Saved %s in %lu us\n", SAVE_FILE, static_cast<unsigned long>(micros() - startUs));
  }

  ttt::Saved saved_{};
  ttt::Rng rng_;
  bool computerOpensNext_ = false;
  // A new game is waiting for NEW GAME to confirm it. Never saved.
  bool confirming_ = false;
  ttt::Mode pendingMode_ = ttt::Mode::TwoPlayers;
  bool loaded_ = false;
  bool saveFailed_ = false;
};

}  // namespace

Game& ticTacToe() {
  static TicTacToe game;
  return game;
}

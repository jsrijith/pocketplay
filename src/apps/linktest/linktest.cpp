#include "linktest.h"

#include <Arduino.h>

#include <cstdio>

#include "core/graphics.h"
#include "core/link/match.h"
#include "core/link/name.h"
#include "core/link/play.h"
#include "core/text.h"
#include "ui/link/link_screens.h"
#include "ui/ui.h"

namespace {

using linkplay::GameId;

// The game: take turns adding one, and whoever reaches kEnd ends it.
constexpr uint8_t kEnd = 10;

struct Counter {
  uint8_t count;
  uint8_t lastBySecond;  // 1 if the player who went second made the last move
};

constexpr int MARGIN = 16;
constexpr int INNER_WIDTH = SCREEN_WIDTH - 2 * MARGIN;
constexpr int LINE_HEIGHT = 36;
constexpr int YOU_TOP = ui::CONTENT_TOP;
constexpr int HEADLINE_TOP = YOU_TOP + LINE_HEIGHT + 24;
constexpr int HEADLINE_HEIGHT = 48;
constexpr int COUNTER_TOP = HEADLINE_TOP + HEADLINE_HEIGHT + 24;
constexpr int COUNTER_HEIGHT = 200;
constexpr int TURN_TOP = COUNTER_TOP + COUNTER_HEIGHT + 16;
constexpr int PILL_TOP = SCREEN_HEIGHT - 16 - ui::PILL_HEIGHT;

class LinkTest final : public App, public linkplay::MatchHost {
 public:
  const char* name() const override { return "LINK TEST"; }

  void open() override {
    match_.leave();
    failed_ = false;
  }

  TapResult onTap(int x, int y) override {
    if (match_.ownsScreen()) {
      switch (linkui::tapAt(match_, x, y)) {
        case linkui::Tap::PlayAgain:
          match_.proposeRematch();
          return TapResult::Redraw;
        case linkui::Tap::Leave:
          match_.leave();
          return TapResult::RedrawFull;
        case linkui::Tap::Nothing:
          return TapResult::Nothing;
      }
      return TapResult::Nothing;
    }

    if (match_.active()) {
      // BACK leaves the match, not the app.
      if (ui::isOnBack(x, y)) {
        match_.leave();
        return TapResult::RedrawFull;
      }
      if (!ui::isInBox(x, y, MARGIN, PILL_TOP, INNER_WIDTH, ui::PILL_HEIGHT)) return TapResult::Nothing;
      return send();
    }

    if (ui::isOnBack(x, y)) return TapResult::Leave;
    if (!ui::isInBox(x, y, MARGIN, PILL_TOP, INNER_WIDTH, ui::PILL_HEIGHT)) return TapResult::Nothing;
    failed_ = !match_.enter(GameId::Test, linkplay::deviceName());
    return TapResult::Redraw;
  }

  bool update() override { return match_.update(millis()); }

  bool busy() const override { return match_.keepAwake(); }

  void draw(uint8_t* frame) override {
    if (match_.ownsScreen()) {
      linkui::draw(frame, match_, name(), resultHeadline());
      return;
    }
    ui::drawBand(frame, name(), nullptr, true);
    char line[48];
    snprintf(line, sizeof(line), "YOU ARE %s", linkplay::deviceName());
    drawTextInBox(frame, Font::Size20, MARGIN, YOU_TOP, INNER_WIDTH, LINE_HEIGHT, line);

    if (!match_.active()) {
      drawTextInBox(frame, Font::Size30, MARGIN, HEADLINE_TOP, INNER_WIDTH, HEADLINE_HEIGHT,
                    failed_ ? "RADIO WOULD NOT START" : "NOT CONNECTED");
      ui::drawPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "FIND A PLAYER", true);
      return;
    }

    snprintf(line, sizeof(line), "PLAYING %s", match_.opponentName());
    drawTextInBox(frame, Font::Size30, MARGIN, HEADLINE_TOP, INNER_WIDTH, HEADLINE_HEIGHT, line);
    drawRectOutline(frame, MARGIN, COUNTER_TOP, INNER_WIDTH, COUNTER_HEIGHT, ui::RULE);
    char count[8];
    snprintf(count, sizeof(count), "%u", state_.count);
    drawTextInBox(frame, Font::Size64, MARGIN, COUNTER_TOP, INNER_WIDTH, COUNTER_HEIGHT, count);

    const char* status = matchGameOver() ? resultHeadline() : match_.yourTurn() ? "YOUR TURN" : "THEIR TURN";
    drawTextInBox(frame, Font::Size20, MARGIN, TURN_TOP, INNER_WIDTH, LINE_HEIGHT, status);
    if (match_.yourTurn() && !matchGameOver()) {
      ui::drawPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "SEND +1", true);
    } else {
      ui::drawDimmedPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "SEND +1");
    }
    match_.notePainted(millis());
  }

  bool saveFailed() const override { return false; }
  void retrySave() override {}

  // MatchHost
  void onMatchStart(bool goesFirst) override {
    state_ = Counter{};
    wentFirst_ = goesFirst;
    Serial.printf("Link test: new game, %s first\n", goesFirst ? "we go" : "they go");
  }
  bool takeOpponentState() override {
    if (!play_.takeOpponent(state_)) return false;
    Serial.printf("Link test: got %u\n", state_.count);
    return true;
  }
  bool matchGameOver() const override { return state_.count >= kEnd; }
  void onMatchEnded() override { Serial.printf("Link test: game over, %s\n", resultHeadline()); }
  void onLinkEnded() override { state_ = Counter{}; }

 private:
  TapResult send() {
    if (!match_.yourTurn() || matchGameOver()) return TapResult::Nothing;
    state_.count++;
    state_.lastBySecond = wentFirst_ ? 0 : 1;
    if (!play_.play(state_)) {
      state_.count--;
      return TapResult::Nothing;
    }
    return TapResult::Redraw;
  }

  const char* resultHeadline() const {
    const bool mine = (state_.lastBySecond == 1) != wentFirst_;
    return mine ? "YOU REACHED 10" : "THEY REACHED 10";
  }

  // The Play before the Match, so it outlives it.
  linkplay::Play<Counter> play_;
  linkplay::Match match_{play_, *this};
  Counter state_{};
  bool wentFirst_ = false;
  bool failed_ = false;
};

LinkTest app;

}  // namespace

App& linkTestApp() { return app; }

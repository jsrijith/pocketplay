#include "linktest.h"

#include <Arduino.h>

#include <cstdio>

#include "core/graphics.h"
#include "core/link/name.h"
#include "core/link/radio.h"
#include "core/link/session.h"
#include "core/text.h"
#include "ui/ui.h"

namespace {

// CrossPlay's id for tests, which no real game uses. Step 3 brings the full table.
constexpr uint16_t kGameId = 0xFF01;

constexpr int MARGIN = 16;
constexpr int INNER_WIDTH = SCREEN_WIDTH - 2 * MARGIN;
constexpr int YOU_TOP = ui::CONTENT_TOP;
constexpr int LINE_HEIGHT = 36;
constexpr int HEADLINE_TOP = YOU_TOP + LINE_HEIGHT + 24;
constexpr int HEADLINE_HEIGHT = 48;
constexpr int COUNTER_TOP = HEADLINE_TOP + HEADLINE_HEIGHT + 24;
constexpr int COUNTER_HEIGHT = 200;
constexpr int TURN_TOP = COUNTER_TOP + COUNTER_HEIGHT + 16;
constexpr int FIRST_TOP = TURN_TOP + LINE_HEIGHT;
constexpr int PILL_TOP = SCREEN_HEIGHT - 16 - ui::PILL_HEIGHT;

using Session = linkplay::Session;

void goodbye();

class LinkTest final : public App {
 public:
  const char* name() const override { return "LINK TEST"; }

  void open() override { stop(); }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) {
      stop();
      return TapResult::Leave;
    }
    if (!ui::isInBox(x, y, MARGIN, PILL_TOP, INNER_WIDTH, ui::PILL_HEIGHT)) return TapResult::Nothing;

    switch (session_.state()) {
      case Session::State::Idle:
      case Session::State::Ended:
        return search();
      case Session::State::Searching:
        stop();
        return TapResult::Redraw;
      case Session::State::Playing:
        return send();
    }
    return TapResult::Nothing;
  }

  bool update() override {
    if (!radio_.started()) return false;
    session_.tick(millis());

    bool changed = false;
    uint8_t payload[2];
    uint8_t length = 0;
    if (session_.takeIncoming(payload, sizeof(payload), length) && length == sizeof(payload)) {
      counter_ = static_cast<uint16_t>(payload[0] | (payload[1] << 8));
      Serial.printf("Link test: got %u from %s\n", counter_, session_.peerName());
      changed = true;
    }

    const Session::State state = session_.state();
    if (state != shownState_) {
      if (state == Session::State::Playing) {
        counter_ = 0;
        wentFirst_ = session_.isHost();
        Serial.printf("Link test: playing %s, %s first\n", session_.peerName(), wentFirst_ ? "we go" : "they go");
      }
      if (state == Session::State::Ended) {
        // They left or went quiet. Nothing more to hear, so the radio goes off now
        // rather than draining the battery.
        Serial.printf("Link test: ended (%d)\n", static_cast<int>(session_.endReason()));
        radio_.end();
        linkplay::setBeforeSleep(nullptr);
      }
      shownState_ = state;
      changed = true;
    }
    if (session_.myTurn() != shownMyTurn_) {
      shownMyTurn_ = session_.myTurn();
      changed = true;
    }
    return changed;
  }

  // Searching or playing: a sleep would end the match.
  bool busy() const override {
    return session_.state() == Session::State::Searching || session_.state() == Session::State::Playing;
  }

  void draw(uint8_t* frame) override {
    ui::drawBand(frame, name(), nullptr, true);

    char line[48];
    snprintf(line, sizeof(line), "YOU ARE %s", linkplay::deviceName());
    drawTextInBox(frame, Font::Size20, MARGIN, YOU_TOP, INNER_WIDTH, LINE_HEIGHT, line);
    drawTextInBox(frame, Font::Size30, MARGIN, HEADLINE_TOP, INNER_WIDTH, HEADLINE_HEIGHT, headline(line, sizeof(line)));

    const Session::State state = session_.state();
    if (state == Session::State::Playing) {
      drawRectOutline(frame, MARGIN, COUNTER_TOP, INNER_WIDTH, COUNTER_HEIGHT, ui::RULE);
      char count[8];
      snprintf(count, sizeof(count), "%u", counter_);
      drawTextInBox(frame, Font::Size64, MARGIN, COUNTER_TOP, INNER_WIDTH, COUNTER_HEIGHT, count);
      drawTextInBox(frame, Font::Size20, MARGIN, TURN_TOP, INNER_WIDTH, LINE_HEIGHT,
                    session_.myTurn() ? "YOUR TURN" : "THEIR TURN");
      drawTextInBox(frame, Font::Size14, MARGIN, FIRST_TOP, INNER_WIDTH, LINE_HEIGHT,
                    wentFirst_ ? "THE COIN TOSS SAID YOU GO FIRST" : "THE COIN TOSS SAID THEY GO FIRST");
    }

    switch (state) {
      case Session::State::Idle:
      case Session::State::Ended:
        ui::drawPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "FIND A PLAYER", true);
        break;
      case Session::State::Searching:
        ui::drawPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "CANCEL", false);
        break;
      case Session::State::Playing:
        if (session_.myTurn()) {
          ui::drawPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "SEND +1", true);
        } else {
          ui::drawDimmedPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "SEND +1");
        }
        break;
    }
  }

  bool saveFailed() const override { return false; }
  void retrySave() override {}

  // Tells the other device, if there is one, then turns the radio off. Safe to call
  // in any state.
  void stop() {
    session_.leave();
    radio_.end();
    session_.reset();
    linkplay::setBeforeSleep(nullptr);
    failed_ = false;
    shownState_ = Session::State::Idle;
    shownMyTurn_ = false;
  }

 private:
  TapResult search() {
    stop();
    if (!radio_.begin()) {
      failed_ = true;
      return TapResult::Redraw;
    }
    session_.begin(radio_, kGameId, linkplay::deviceName(), millis());
    linkplay::setBeforeSleep(&goodbye);
    shownState_ = session_.state();
    return TapResult::Redraw;
  }

  TapResult send() {
    if (!session_.myTurn()) return TapResult::Nothing;
    const uint16_t next = static_cast<uint16_t>(counter_ + 1);
    const uint8_t payload[2] = {static_cast<uint8_t>(next), static_cast<uint8_t>(next >> 8)};
    if (!session_.submit(payload, sizeof(payload))) return TapResult::Nothing;
    counter_ = next;
    shownMyTurn_ = session_.myTurn();
    return TapResult::Redraw;
  }

  const char* headline(char* buffer, size_t size) const {
    if (failed_) return "RADIO WOULD NOT START";
    switch (session_.state()) {
      case Session::State::Idle:
        return "NOT CONNECTED";
      case Session::State::Searching:
        return "LOOKING FOR A PLAYER";
      case Session::State::Playing:
        snprintf(buffer, size, "PLAYING %s", session_.peerName());
        return buffer;
      case Session::State::Ended:
        switch (session_.endReason()) {
          case Session::EndReason::PeerLeft:
            return "THEY LEFT";
          case Session::EndReason::PeerLost:
            return "LOST THEM";
          case Session::EndReason::LocalLeft:
          case Session::EndReason::None:
            return "NOT CONNECTED";
        }
    }
    return "";
  }

  linkplay::Radio radio_;
  Session session_;
  uint16_t counter_ = 0;
  bool wentFirst_ = false;
  bool failed_ = false;
  // What the screen last showed, so update() redraws only when something changes.
  Session::State shownState_ = Session::State::Idle;
  bool shownMyTurn_ = false;
};

LinkTest app;

void goodbye() { app.stop(); }

}  // namespace

App& linkTestApp() { return app; }

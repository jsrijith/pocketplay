#include "link_screens.h"

#include "core/graphics.h"
#include "core/text.h"
#include "ui/ui.h"

namespace linkui {
namespace {

using linkplay::Match;
using linkplay::SeatState;

constexpr int MARGIN = 16;
constexpr int INNER_WIDTH = SCREEN_WIDTH - 2 * MARGIN;

constexpr int HEADLINE_TOP = ui::CONTENT_TOP;
constexpr int HEADLINE_HEIGHT = 56;

// The two seats, in one outlined box with a rail down its left edge: grey while
// looking, solid once the two devices are linked.
constexpr int SEATS_TOP = HEADLINE_TOP + HEADLINE_HEIGHT + 16;
constexpr int SEAT_HEIGHT = ui::ROW_HEIGHT;
constexpr int SEATS_HEIGHT = 2 * SEAT_HEIGHT;
constexpr int RAIL_WIDTH = 8;
constexpr int SEAT_TEXT_LEFT = MARGIN + RAIL_WIDTH + 16;
constexpr int SEAT_TEXT_RIGHT = MARGIN + INNER_WIDTH - 16;

// The radio mark beside LOOKING FOR A PLAYER.
constexpr int MARK_RADIUS = 18;
constexpr int MARK_X = MARGIN + INNER_WIDTH - MARK_RADIUS - 4;
constexpr int MARK_Y = HEADLINE_TOP + HEADLINE_HEIGHT / 2;

constexpr int PILL_TOP = SCREEN_HEIGHT - 16 - ui::PILL_HEIGHT;
constexpr int HALF_WIDTH = (INNER_WIDTH - ui::GAP) / 2;
constexpr int RIGHT_HALF_LEFT = MARGIN + HALF_WIDTH + ui::GAP;

enum class Footer {
  Cancel,     // looking: one outlined CANCEL
  Offer,      // ANOTHER GAME?: LEAVE and PLAY AGAIN
  Done,       // they've gone: one DONE
};

bool searching(const Match& match) { return !match.linked(); }

Footer footerFor(const Match& match) {
  if (searching(match)) return Footer::Cancel;
  if (match.offerPlayAgain()) return Footer::Offer;
  return Footer::Done;
}

const char* seatValue(SeatState state, bool linked) {
  switch (state) {
    case SeatState::Looking:
      return linked ? "WAITING" : "LOOKING";
    case SeatState::Deciding:
      return "DECIDING";
    case SeatState::Ready:
      return "READY";
    case SeatState::Left:
      return "LEFT";
    case SeatState::Lost:
      return "LOST";
  }
  return "";
}

void drawSeat(uint8_t* frame, int top, const char* label, const char* value) {
  ui::drawTextLeft(frame, Font::Size20, SEAT_TEXT_LEFT, top, SEAT_HEIGHT, label);
  const int baseline = top + (SEAT_HEIGHT + capHeight(Font::Size20)) / 2;
  drawText(frame, Font::Size20, SEAT_TEXT_RIGHT, baseline, value, Align::Right);
}

void drawRail(uint8_t* frame, bool linked) {
  const int left = MARGIN + ui::HAIRLINE;
  const int top = SEATS_TOP + ui::HAIRLINE;
  const int height = SEATS_HEIGHT - 2 * ui::HAIRLINE;
  if (linked) {
    fillRect(frame, left, top, RAIL_WIDTH, height, true);
    return;
  }
  for (int y = top; y < top + height; y++) {
    for (int x = left; x < left + RAIL_WIDTH; x++) {
      if (x % 2 == 0 && y % 2 == 0) setPixel(frame, x, y, true);
    }
  }
}

}  // namespace

void draw(uint8_t* frame, const Match& match, const char* gameTitle, const char* gameHeadline) {
  ui::drawBand(frame, gameTitle, nullptr, true);

  const bool looking = searching(match);
  const char* headline = looking ? "LOOKING FOR A PLAYER" : match.interruption();
  // A finished game keeps its own result, such as YOU WIN, even after they've gone:
  // their seat says LEFT.
  if (headline == nullptr) headline = gameHeadline;
  // The largest cut that fits beside the radio mark, never shortened.
  const int headlineWidth = INNER_WIDTH - (looking ? 2 * MARK_RADIUS + 16 : 0);
  Font font = Font::Size30;
  if (textWidth(font, headline) > headlineWidth) font = Font::Size20;
  if (textWidth(font, headline) > headlineWidth) font = Font::Size14;
  ui::drawTextLeft(frame, font, MARGIN, HEADLINE_TOP, HEADLINE_HEIGHT, headline);
  if (looking) {
    fillRect(frame, MARK_X - 3, MARK_Y - 3, 6, 6, true);
    drawRing(frame, MARK_X, MARK_Y, MARK_RADIUS / 2, 2, true);
    drawRing(frame, MARK_X, MARK_Y, MARK_RADIUS, 2, true);
  }

  drawRectOutline(frame, MARGIN, SEATS_TOP, INNER_WIDTH, SEATS_HEIGHT, ui::HAIRLINE);
  drawRail(frame, match.linked());
  fillRect(frame, MARGIN + RAIL_WIDTH, SEATS_TOP + SEAT_HEIGHT, INNER_WIDTH - RAIL_WIDTH, ui::HAIRLINE, true);
  drawSeat(frame, SEATS_TOP, "YOU", seatValue(match.you(), match.linked()));
  const char* them = match.opponentName();
  drawSeat(frame, SEATS_TOP + SEAT_HEIGHT, them != nullptr && them[0] != '\0' ? them : "- - - -",
           seatValue(match.them(), match.linked()));

  switch (footerFor(match)) {
    case Footer::Cancel:
      ui::drawPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "CANCEL", false);
      break;
    case Footer::Offer:
      ui::drawPill(frame, MARGIN, PILL_TOP, HALF_WIDTH, "LEAVE", false);
      // Once you've said yes, the button has nothing left to do until they answer.
      if (match.you() == SeatState::Ready) {
        ui::drawDimmedPill(frame, RIGHT_HALF_LEFT, PILL_TOP, HALF_WIDTH, "PLAY AGAIN");
      } else {
        ui::drawPill(frame, RIGHT_HALF_LEFT, PILL_TOP, HALF_WIDTH, "PLAY AGAIN", true);
      }
      break;
    case Footer::Done:
      ui::drawPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "DONE", true);
      break;
  }
}

Tap tapAt(const Match& match, int x, int y) {
  if (ui::isOnBack(x, y)) return Tap::Leave;
  switch (footerFor(match)) {
    case Footer::Cancel:
    case Footer::Done:
      return ui::isInBox(x, y, MARGIN, PILL_TOP, INNER_WIDTH, ui::PILL_HEIGHT) ? Tap::Leave : Tap::Nothing;
    case Footer::Offer:
      if (ui::isInBox(x, y, MARGIN, PILL_TOP, HALF_WIDTH, ui::PILL_HEIGHT)) return Tap::Leave;
      if (match.you() != SeatState::Ready && ui::isInBox(x, y, RIGHT_HALF_LEFT, PILL_TOP, HALF_WIDTH, ui::PILL_HEIGHT)) {
        return Tap::PlayAgain;
      }
      return Tap::Nothing;
  }
  return Tap::Nothing;
}

}  // namespace linkui

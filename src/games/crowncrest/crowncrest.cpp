#include "crowncrest.h"

#include <Arduino.h>
#include <esp_random.h>

#include <cstdio>

#include "core/graphics.h"
#include "core/storage.h"
#include "core/text.h"
#include "crowncrest_faces.h"
#include "crowncrest_icon.h"
#include "crowncrest_rules.h"
#include "crowncrest_save.h"
#include "ui/ui.h"

namespace cc = crowncrest;
namespace faces = crowncrestFaces;

namespace {

constexpr const char* SAVE_FILE = "crowncrest";

// A status capsule saying whose move it is, four big dice in a 2 x 2 grid with a label under
// each naming the die and who took it, then UNDO and ROLL along the bottom.
constexpr int MARGIN = 16;
constexpr int GAP = 12;
constexpr int INNER_WIDTH = SCREEN_WIDTH - 2 * MARGIN;
constexpr int STATUS_TOP = ui::CONTENT_TOP;
constexpr int STATUS_HEIGHT = 44;
constexpr int DIE_GAP = 24;
constexpr int DIE = (INNER_WIDTH - DIE_GAP) / 2;  // 212: as big as the margins allow
constexpr int DIE_RADIUS = 28;
constexpr int LABEL_GAP = 4;
constexpr int LABEL_HEIGHT = 28;
constexpr int ROW_GAP = 16;
constexpr int BUTTON_TOP = SCREEN_HEIGHT - MARGIN - ui::PILL_HEIGHT;
// The grid, centred in the room between the capsule and the buttons.
constexpr int CELL_HEIGHT = DIE + LABEL_GAP + LABEL_HEIGHT;
constexpr int GRID_HEIGHT = 2 * CELL_HEIGHT + ROW_GAP;
constexpr int ROOM_TOP = STATUS_TOP + STATUS_HEIGHT + GAP;
constexpr int GRID_TOP = ROOM_TOP + (BUTTON_TOP - GAP - ROOM_TOP - GRID_HEIGHT) / 2;
constexpr int UNDO_WIDTH = (INNER_WIDTH - GAP) / 3;
constexpr int ROLL_LEFT = MARGIN + UNDO_WIDTH + GAP;
constexpr int ROLL_WIDTH = INNER_WIDTH - UNDO_WIDTH - GAP;

static_assert(faces::FACE + 2 * ui::FRAME <= DIE, "a face must fit inside its die");
static_assert(GRID_TOP >= ROOM_TOP, "the dice must start below the capsule");

int dieLeft(int d) { return (SCREEN_WIDTH - 2 * DIE - DIE_GAP) / 2 + (d % 2) * (DIE + DIE_GAP); }
int dieTop(int d) { return GRID_TOP + (d / 2) * (CELL_HEIGHT + ROW_GAP); }

const char* playerName(uint8_t p) { return p == 0 ? "A" : "B"; }

class CrownCrest final : public Game {
 public:
  const char* name() const override { return "CROWN & CREST"; }
  const uint8_t* icon() const override { return crowncrestIcon::ICON; }

  void open() override {
    if (loaded_) return;
    loaded_ = true;
    load();
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) return TapResult::Leave;
    if (ui::isInBox(x, y, ROLL_LEFT, BUTTON_TOP, ROLL_WIDTH, ui::PILL_HEIGHT)) return rollDice();
    if (ui::isInBox(x, y, MARGIN, BUTTON_TOP, UNDO_WIDTH, ui::PILL_HEIGHT)) {
      return cc::undoPick(turn_) ? changed() : TapResult::Nothing;
    }
    // A die and its label are one target.
    for (int d = 0; d < cc::kDice; d++) {
      if (ui::isInBox(x, y, dieLeft(d), dieTop(d), DIE, CELL_HEIGHT)) {
        return cc::pick(turn_, d) ? changed() : TapResult::Nothing;
      }
    }
    return TapResult::Nothing;
  }

  // The Down button rolls when a roll is due, for rolling without looking.
  TapResult onDownPressed() override { return rollDice(); }

  void draw(uint8_t* frame) override {
    ui::drawBand(frame, name(), nullptr, true);
    drawStatus(frame);
    const bool rolled = cc::rolled(turn_.dice);
    for (int d = 0; d < cc::kDice; d++) {
      const int left = dieLeft(d), top = dieTop(d);
      if (rolled) {
        drawRoundRectOutline(frame, left, top, DIE, DIE, DIE_RADIUS, ui::FRAME);
        drawIcon(frame, faces::FACES[d][turn_.dice.face[d] - 1], faces::FACE, faces::FACE,
                 left + (DIE - faces::FACE) / 2, top + (DIE - faces::FACE) / 2);
      } else {
        // Before the first roll the dice are there, dimmed and blank.
        ui::drawDimmedPill(frame, left, top, DIE, "", DIE);
      }
      drawLabel(frame, d);
    }

    if (turn_.picks == 0) {
      ui::drawDimmedPill(frame, MARGIN, BUTTON_TOP, UNDO_WIDTH, "UNDO");
    } else {
      ui::drawPill(frame, MARGIN, BUTTON_TOP, UNDO_WIDTH, "UNDO", false);
    }
    char roll[16];
    std::snprintf(roll, sizeof(roll), "%s ROLLS", playerName(cc::nextRoller(turn_)));
    if (cc::canRoll(turn_)) {
      ui::drawPill(frame, ROLL_LEFT, BUTTON_TOP, ROLL_WIDTH, roll, true);
    } else {
      ui::drawDimmedPill(frame, ROLL_LEFT, BUTTON_TOP, ROLL_WIDTH, "ROLL");
    }
  }

  bool saveFailed() const override { return saveFailed_; }
  void retrySave() override { persist(); }

 private:
  TapResult changed() {
    persist();
    return TapResult::Redraw;
  }

  // Each roll stirs in the chip's random number and the microsecond of the tap. With the
  // radio off the chip's number alone isn't guaranteed random, but a person's timing is.
  TapResult rollDice() {
    const uint64_t entropy = (static_cast<uint64_t>(esp_random()) << 32) ^ static_cast<uint64_t>(micros());
    return cc::rollTurn(turn_, source_, entropy) ? changed() : TapResult::Nothing;
  }

  // Whose move it is, in words. Solid once all four dice are shared out, so the end of a turn
  // stands out from the picking.
  void drawStatus(uint8_t* frame) const {
    char text[40];
    const uint8_t p = cc::picker(turn_);
    if (!cc::rolled(turn_.dice)) {
      std::snprintf(text, sizeof(text), "PLAYER A ROLLS FIRST");
    } else if (cc::done(turn_)) {
      std::snprintf(text, sizeof(text), "PLAYER %s ROLLS NEXT", playerName(cc::nextRoller(turn_)));
    } else if (turn_.picks == 0) {
      std::snprintf(text, sizeof(text), "PLAYER %s: TAKE 1 DIE", playerName(p));
    } else if (turn_.picks == 1) {
      std::snprintf(text, sizeof(text), "PLAYER %s: TAKE 2 DICE", playerName(p));
    } else {
      std::snprintf(text, sizeof(text), "PLAYER %s: TAKE 1 MORE", playerName(p));
    }
    const bool solid = cc::done(turn_);
    if (solid) {
      fillRect(frame, MARGIN, STATUS_TOP, INNER_WIDTH, STATUS_HEIGHT, true);
    } else {
      drawRectOutline(frame, MARGIN, STATUS_TOP, INNER_WIDTH, STATUS_HEIGHT, 2);
    }
    drawTextInBox(frame, Font::Size20, MARGIN, STATUS_TOP, INNER_WIDTH, STATUS_HEIGHT, text, !solid);
  }

  // "DIE 3" while free; once taken, a black label naming the owner.
  void drawLabel(uint8_t* frame, int d) const {
    const int left = dieLeft(d), top = dieTop(d) + DIE + LABEL_GAP;
    const uint8_t who = cc::owner(turn_, d);
    char label[32];
    if (who == cc::kNoOne) {
      std::snprintf(label, sizeof(label), "DIE %d", d + 1);
      drawTextInBox(frame, Font::Size14, left, top, DIE, LABEL_HEIGHT, label);
      return;
    }
    std::snprintf(label, sizeof(label), "DIE %d: PLAYER %s", d + 1, playerName(who));
    fillRoundRect(frame, left, top, DIE, LABEL_HEIGHT, LABEL_HEIGHT / 2, true);
    drawTextInBox(frame, Font::Size14, left, top, DIE, LABEL_HEIGHT, label, false);
  }

  void load() {
    source_.stir((static_cast<uint64_t>(esp_random()) << 32) | esp_random());
    if (!storage::ready()) return;
    uint8_t data[cc::SAVE_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (storage::load(SAVE_FILE, version, data, sizeof(data), size) && !cc::unpack(version, data, size, turn_)) {
      Serial.println("Ignoring saved Crown & Crest turn");
    }
  }

  // The turn, saved on every roll and pick, so a wake shows the same dice and picks.
  void persist() {
    if (!storage::ready()) {
      saveFailed_ = false;
      return;
    }
    uint8_t data[cc::SAVE_BYTES];
    const size_t size = cc::pack(turn_, data);
    saveFailed_ = !storage::save(SAVE_FILE, cc::SAVE_VERSION, data, size);
  }

  cc::Turn turn_{};
  cc::Source source_{};
  bool loaded_ = false;
  bool saveFailed_ = false;
};

}  // namespace

Game& crownCrestGame() {
  static CrownCrest game;
  return game;
}

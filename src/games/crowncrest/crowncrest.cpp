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

// Four big dice in a 2 x 2 grid, each named under it, and ROLL along the bottom.
constexpr int MARGIN = 16;
constexpr int GAP = 12;
constexpr int INNER_WIDTH = SCREEN_WIDTH - 2 * MARGIN;
constexpr int DIE_GAP = 24;
constexpr int DIE = (INNER_WIDTH - DIE_GAP) / 2;  // 212: as big as the margins allow
constexpr int DIE_RADIUS = 28;
constexpr int LABEL_GAP = 4;
constexpr int LABEL_HEIGHT = 28;
constexpr int ROW_GAP = 16;
constexpr int BUTTON_TOP = SCREEN_HEIGHT - MARGIN - ui::PILL_HEIGHT;
// The grid, centred in the room between the band and the button.
constexpr int CELL_HEIGHT = DIE + LABEL_GAP + LABEL_HEIGHT;
constexpr int GRID_HEIGHT = 2 * CELL_HEIGHT + ROW_GAP;
constexpr int GRID_TOP = ui::CONTENT_TOP + (BUTTON_TOP - GAP - ui::CONTENT_TOP - GRID_HEIGHT) / 2;

static_assert(faces::FACE + 2 * ui::FRAME <= DIE, "a face must fit inside its die");
static_assert(GRID_TOP >= ui::CONTENT_TOP, "the dice must start below the band");

int dieLeft(int d) { return (SCREEN_WIDTH - 2 * DIE - DIE_GAP) / 2 + (d % 2) * (DIE + DIE_GAP); }
int dieTop(int d) { return GRID_TOP + (d / 2) * (CELL_HEIGHT + ROW_GAP); }

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
    if (ui::isInBox(x, y, MARGIN, BUTTON_TOP, INNER_WIDTH, ui::PILL_HEIGHT)) return rollDice();
    return TapResult::Nothing;
  }

  // The Down button rolls too, for rolling without looking.
  TapResult onDownPressed() override { return rollDice(); }

  void draw(uint8_t* frame) override {
    ui::drawBand(frame, name(), nullptr, true);
    const bool rolled = cc::rolled(roll_);
    for (int d = 0; d < cc::kDice; d++) {
      const int left = dieLeft(d), top = dieTop(d);
      if (rolled) {
        drawRoundRectOutline(frame, left, top, DIE, DIE, DIE_RADIUS, ui::FRAME);
        drawIcon(frame, faces::FACES[d][roll_.face[d] - 1], faces::FACE, faces::FACE, left + (DIE - faces::FACE) / 2,
                 top + (DIE - faces::FACE) / 2);
      } else {
        // Before the first roll the dice are there, dimmed and blank.
        ui::drawDimmedPill(frame, left, top, DIE, "", DIE);
      }
      char label[8];
      std::snprintf(label, sizeof(label), "DIE %d", d + 1);
      drawTextInBox(frame, Font::Size14, left, top + DIE + LABEL_GAP, DIE, LABEL_HEIGHT, label);
    }
    ui::drawPill(frame, MARGIN, BUTTON_TOP, INNER_WIDTH, "ROLL", true);
  }

  bool saveFailed() const override { return saveFailed_; }
  void retrySave() override { persist(); }

 private:
  // Each roll stirs in the chip's random number and the microsecond of the tap. With the
  // radio off the chip's number alone isn't guaranteed random, but a person's timing is.
  TapResult rollDice() {
    const uint64_t entropy = (static_cast<uint64_t>(esp_random()) << 32) ^ static_cast<uint64_t>(micros());
    cc::roll(roll_, source_, entropy);
    persist();
    return TapResult::Redraw;
  }

  void load() {
    source_.stir((static_cast<uint64_t>(esp_random()) << 32) | esp_random());
    if (!storage::ready()) return;
    uint8_t data[cc::SAVE_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (storage::load(SAVE_FILE, version, data, sizeof(data), size) && !cc::unpack(version, data, size, roll_)) {
      Serial.println("Ignoring saved Crown & Crest roll");
    }
  }

  // The last roll, so a wake shows the same dice.
  void persist() {
    if (!storage::ready()) {
      saveFailed_ = false;
      return;
    }
    uint8_t data[cc::SAVE_BYTES];
    const size_t size = cc::pack(roll_, data);
    saveFailed_ = !storage::save(SAVE_FILE, cc::SAVE_VERSION, data, size);
  }

  cc::Roll roll_{};
  cc::Source source_{};
  bool loaded_ = false;
  bool saveFailed_ = false;
};

}  // namespace

Game& crownCrestGame() {
  static CrownCrest game;
  return game;
}

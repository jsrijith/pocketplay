#include "karubatiles.h"

#include <Arduino.h>
#include <esp_random.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/graphics.h"
#include "core/storage.h"
#include "core/text.h"
#include "karubatiles_icon.h"
#include "karubatiles_rules.h"
#include "karubatiles_save.h"
#include "ui/ui.h"

namespace kt = karubatiles;

namespace {

constexpr const char* SAVE_FILE = "karubatiles";

// From the top: the drawn tile on the left, with its number big, its treasure and the count beside
// it; the three draws before it; the board of all 36; then NEW GAME and DRAW along the bottom.
constexpr int MARGIN = 16;
constexpr int GAP = 12;
constexpr int INNER_WIDTH = SCREEN_WIDTH - 2 * MARGIN;
constexpr int BIG_TOP = ui::CONTENT_TOP;
constexpr int TILE = 216;
constexpr int BIG_HEIGHT = TILE + 20;
constexpr int TILE_LEFT = MARGIN;
constexpr int TILE_TOP = BIG_TOP + (BIG_HEIGHT - TILE) / 2;
constexpr int BEFORE_TOP = BIG_TOP + BIG_HEIGHT + 8;
constexpr int BEFORE_HEIGHT = 32;
constexpr int BOARD_TOP = BEFORE_TOP + BEFORE_HEIGHT + 8;
constexpr int BUTTON_TOP = SCREEN_HEIGHT - MARGIN - ui::PILL_HEIGHT;
constexpr int BOARD_BOTTOM = BUTTON_TOP - GAP;
constexpr int NEW_WIDTH = (INNER_WIDTH - GAP) / 3;
constexpr int DRAW_LEFT = MARGIN + NEW_WIDTH + GAP;
constexpr int DRAW_WIDTH = INNER_WIDTH - NEW_WIDTH - GAP;
constexpr int BEFORE_COUNT = 3;

// Beside the tile: the number as big as the font goes, then CRYSTAL or GOLD, then the count,
// anchored to the tile's bottom edge.
constexpr int SIDE_LEFT = TILE_LEFT + TILE + GAP;
constexpr int SIDE_WIDTH = SCREEN_WIDTH - MARGIN - SIDE_LEFT;
constexpr int NUMBER_HEIGHT = 110;
constexpr int TREASURE_TOP = TILE_TOP + NUMBER_HEIGHT;
constexpr int TREASURE_HEIGHT = 44;
constexpr int COUNT_HEIGHT = 36;
constexpr int COUNT_TOP = TILE_TOP + TILE - COUNT_HEIGHT;

static_assert(TREASURE_TOP + TREASURE_HEIGHT <= COUNT_TOP, "the treasure must end above the count");

// The board: 6 by 6, 1-6 on the top row, as the tiles are numbered.
constexpr int COLUMNS = 6;
constexpr int ROWS = kt::kTiles / COLUMNS;
constexpr int CELL_WIDTH = INNER_WIDTH / COLUMNS;
constexpr int CELL_HEIGHT = (BOARD_BOTTOM - BOARD_TOP) / ROWS;
constexpr int BOARD_LEFT = MARGIN + (INNER_WIDTH - CELL_WIDTH * COLUMNS) / 2;

static_assert(CELL_HEIGHT >= 50, "the board's rows must be tall enough for Size30");

// The tile, in its own 0..TILE-1 square: paths PATH wide through the middle, edged in black, on
// a jungle of grey dither. The number sits in a circle in the top-left corner, as on the printed
// tile, and a treasure in the bottom-right one. Those two corners are always jungle, because
// every path runs through the middle.
constexpr int FRAME = ui::RULE;
constexpr int PATH = 56;
constexpr int PATH_LO = (TILE - PATH) / 2;
constexpr int PATH_HI = PATH_LO + PATH;
constexpr int PATH_EDGE = 3;
constexpr int CORNER_CENTRE = (FRAME + PATH_LO) / 2;           // of the top-left corner
constexpr int FAR_CORNER_CENTRE = (PATH_HI + TILE - FRAME) / 2;  // of the bottom-right one
constexpr int NUMBER_RADIUS = 35;
constexpr int GEM = 26;  // the treasure diamond's half-height
constexpr int GEM_EDGE = 4;
constexpr int GEM_CLEAR = 5;  // white round the diamond, so the dither doesn't eat its edge

static_assert(CORNER_CENTRE - NUMBER_RADIUS >= FRAME, "the number's circle must fit its corner");
static_assert(FAR_CORNER_CENTRE + GEM + GEM_CLEAR <= TILE - FRAME, "the treasure must fit its corner");

bool inPath(uint8_t paths, int x, int y) {
  if (paths == 0) return false;
  const bool acrossMiddle = x >= PATH_LO && x < PATH_HI;
  const bool downMiddle = y >= PATH_LO && y < PATH_HI;
  if (acrossMiddle && downMiddle) return true;
  if (acrossMiddle && y < PATH_LO) return paths & kt::kNorth;
  if (acrossMiddle && y >= PATH_HI) return paths & kt::kSouth;
  if (downMiddle && x >= PATH_HI) return paths & kt::kEast;
  if (downMiddle && x < PATH_LO) return paths & kt::kWest;
  return false;
}

// A path pixel is edge if a neighbour PATH_EDGE away, still on the tile, is jungle. Where a path
// leaves the tile it stays open, so it reads as running on to the next tile.
bool onPathEdge(uint8_t paths, int x, int y) {
  const int dx[] = {PATH_EDGE, -PATH_EDGE, 0, 0};
  const int dy[] = {0, 0, PATH_EDGE, -PATH_EDGE};
  for (int i = 0; i < 4; i++) {
    const int nx = x + dx[i];
    const int ny = y + dy[i];
    if (nx < 0 || ny < 0 || nx >= TILE || ny >= TILE) continue;
    if (!inPath(paths, nx, ny)) return true;
  }
  return false;
}

void drawTile(uint8_t* frame, int number) {
  const kt::Tile& t = kt::tile(number);
  for (int y = 0; y < TILE; y++) {
    for (int x = 0; x < TILE; x++) {
      const int sx = TILE_LEFT + x;
      const int sy = TILE_TOP + y;
      bool black;
      if (inPath(t.paths, x, y)) {
        black = onPathEdge(t.paths, x, y);
      } else {
        black = sx % 2 == 0 && sy % 2 == 0;  // the one grey: one dot in four
      }
      setPixel(frame, sx, sy, black);
    }
  }
  drawRectOutline(frame, TILE_LEFT, TILE_TOP, TILE, TILE, FRAME);

  // The number in a white disc with a ring round it.
  const int cx = TILE_LEFT + CORNER_CENTRE;
  const int cy = TILE_TOP + CORNER_CENTRE;
  for (int dy = -NUMBER_RADIUS; dy <= NUMBER_RADIUS; dy++) {
    for (int dx = -NUMBER_RADIUS; dx <= NUMBER_RADIUS; dx++) {
      if (dx * dx + dy * dy <= NUMBER_RADIUS * NUMBER_RADIUS) setPixel(frame, cx + dx, cy + dy, false);
    }
  }
  drawRing(frame, cx, cy, NUMBER_RADIUS, ui::RULE, true);
  char text[4];
  std::snprintf(text, sizeof(text), "%d", number);
  drawTextInBox(frame, Font::Size30, cx - NUMBER_RADIUS, cy - NUMBER_RADIUS, 2 * NUMBER_RADIUS, 2 * NUMBER_RADIUS, text);

  // A crystal is an outlined diamond, gold a solid one.
  if (t.treasure == kt::Treasure::None) return;
  const int gx = TILE_LEFT + FAR_CORNER_CENTRE;
  const int gy = TILE_TOP + FAR_CORNER_CENTRE;
  const int clear = GEM + GEM_CLEAR;
  for (int dy = -clear; dy <= clear; dy++) {
    for (int dx = -clear; dx <= clear; dx++) {
      const int d = std::abs(dx) + std::abs(dy);
      if (d > clear) continue;
      bool black = false;
      if (d <= GEM) black = t.treasure == kt::Treasure::Gold || d > GEM - GEM_EDGE;
      setPixel(frame, gx + dx, gy + dy, black);
    }
  }
}

class KarubaTiles final : public Game {
 public:
  const char* name() const override { return "KARUBA TILES"; }
  const uint8_t* icon() const override { return karubatilesIcon::ICON; }

  void open() override {
    if (!loaded_) {
      loaded_ = true;
      load();
    }
    newArmed_ = false;
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) {
      newArmed_ = false;
      return TapResult::Leave;
    }
    if (ui::isInBox(x, y, DRAW_LEFT, BUTTON_TOP, DRAW_WIDTH, ui::PILL_HEIGHT)) return drawNext();
    if (ui::isInBox(x, y, MARGIN, BUTTON_TOP, NEW_WIDTH, ui::PILL_HEIGHT)) {
      if (state_.count == 0) return TapResult::Nothing;  // dimmed: nothing to clear
      if (!newArmed_) {
        newArmed_ = true;
        return TapResult::Redraw;
      }
      state_ = kt::State{};
      newArmed_ = false;
      persist();
      return TapResult::RedrawFull;
    }
    return disarm();
  }

  // The Down button draws too, for drawing without looking.
  TapResult onDownPressed() override { return drawNext(); }

  void draw(uint8_t* frame) override {
    ui::drawBand(frame, name(), nullptr, true);
    drawSide(frame);

    const int last = kt::last(state_);
    if (last == 0) {
      // An empty tile, the same size, so the first draw doesn't move anything.
      drawRectOutline(frame, TILE_LEFT, TILE_TOP, TILE, TILE, FRAME);
      drawTextInBox(frame, Font::Size20, TILE_LEFT, TILE_TOP, TILE, TILE, "TAP DRAW");
    } else {
      drawTile(frame, last);
    }

    if (state_.count >= 2) {
      char before[64] = "BEFORE:";
      for (int i = state_.count - 2, shown = 0; i >= 0 && shown < BEFORE_COUNT; i--, shown++) {
        char number[8];
        std::snprintf(number, sizeof(number), "   %d", state_.order[i]);
        std::strncat(before, number, sizeof(before) - std::strlen(before) - 1);
      }
      ui::drawTextLeft(frame, Font::Size20, MARGIN + 4, BEFORE_TOP, BEFORE_HEIGHT, before);
    }

    drawBoard(frame, last);

    if (state_.count == 0) {
      ui::drawDimmedPill(frame, MARGIN, BUTTON_TOP, NEW_WIDTH, "NEW GAME");
    } else {
      ui::drawPill(frame, MARGIN, BUTTON_TOP, NEW_WIDTH, newArmed_ ? "SURE?" : "NEW GAME", false);
    }
    if (kt::finished(state_)) {
      ui::drawDimmedPill(frame, DRAW_LEFT, BUTTON_TOP, DRAW_WIDTH, "ALL DRAWN");
    } else {
      ui::drawPill(frame, DRAW_LEFT, BUTTON_TOP, DRAW_WIDTH, "DRAW", true);
    }
  }

  bool saveFailed() const override { return saveFailed_; }
  void retrySave() override { persist(); }

 private:
  TapResult drawNext() {
    if (kt::finished(state_)) return disarm();  // dimmed: every tile is out
    const uint64_t entropy = (static_cast<uint64_t>(esp_random()) << 32) ^ static_cast<uint64_t>(micros());
    kt::draw(state_, source_, entropy);
    newArmed_ = false;
    persist();
    // The last tile ends the round of draws, so it gets the full refresh.
    return kt::finished(state_) ? TapResult::RedrawFull : TapResult::Redraw;
  }

  void drawSide(uint8_t* frame) {
    const int last = kt::last(state_);
    if (last != 0) {
      char number[4];
      std::snprintf(number, sizeof(number), "%d", last);
      drawTextInBox(frame, Font::Size64, SIDE_LEFT, TILE_TOP, SIDE_WIDTH, NUMBER_HEIGHT, number);
      const kt::Treasure treasure = kt::tile(last).treasure;
      if (treasure != kt::Treasure::None) {
        drawTextInBox(frame, Font::Size30, SIDE_LEFT, TREASURE_TOP, SIDE_WIDTH, TREASURE_HEIGHT,
                      treasure == kt::Treasure::Crystal ? "CRYSTAL" : "GOLD");
      }
    }
    char count[16];
    std::snprintf(count, sizeof(count), "%d/%d", state_.count, kt::kTiles);
    drawTextInBox(frame, Font::Size30, SIDE_LEFT, COUNT_TOP, SIDE_WIDTH, COUNT_HEIGHT, count);
  }

  // A tap anywhere else takes back a NEW GAME that was waiting for its second tap.
  TapResult disarm() {
    if (!newArmed_) return TapResult::Nothing;
    newArmed_ = false;
    return TapResult::Redraw;
  }

  // Drawn tiles are black squares; the latest is a white square in a thick frame instead, so it
  // stands out from the rest.
  void drawBoard(uint8_t* frame, int last) {
    for (int number = 1; number <= kt::kTiles; number++) {
      const int x = BOARD_LEFT + ((number - 1) % COLUMNS) * CELL_WIDTH;
      const int y = BOARD_TOP + ((number - 1) / COLUMNS) * CELL_HEIGHT;
      char text[4];
      std::snprintf(text, sizeof(text), "%d", number);
      if (number == last) {
        drawRectOutline(frame, x, y, CELL_WIDTH + 1, CELL_HEIGHT + 1, ui::FRAME);
        drawTextInBox(frame, Font::Size30, x, y, CELL_WIDTH, CELL_HEIGHT, text);
      } else if (kt::isDrawn(state_, number)) {
        fillRect(frame, x, y, CELL_WIDTH + 1, CELL_HEIGHT + 1, true);
        drawTextInBox(frame, Font::Size30, x, y, CELL_WIDTH, CELL_HEIGHT, text, false);
      } else {
        drawRectOutline(frame, x, y, CELL_WIDTH + 1, CELL_HEIGHT + 1, ui::HAIRLINE);
        drawTextInBox(frame, Font::Size30, x, y, CELL_WIDTH, CELL_HEIGHT, text);
      }
    }
  }

  // ---- Saves ----

  void load() {
    source_.stir((static_cast<uint64_t>(esp_random()) << 32) | esp_random());
    if (!storage::ready()) return;
    uint8_t data[kt::MAX_SAVE_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(SAVE_FILE, version, data, sizeof(data), size)) return;
    if (!kt::unpack(version, data, size, state_)) Serial.println("Ignoring saved Karuba Tiles draws");
  }

  // Every draw is saved as it's made. Nothing drawn is nothing worth keeping, so NEW GAME erases
  // the save.
  void persist() {
    if (!storage::ready()) {
      saveFailed_ = false;
      return;
    }
    if (state_.count == 0) {
      saveFailed_ = !storage::erase(SAVE_FILE);
      return;
    }
    uint8_t data[kt::MAX_SAVE_BYTES];
    const size_t size = kt::pack(state_, data);
    saveFailed_ = !storage::save(SAVE_FILE, kt::SAVE_VERSION, data, size);
  }

  kt::State state_{};
  kt::Source source_{};
  bool newArmed_ = false;
  bool loaded_ = false;
  bool saveFailed_ = false;
};

}  // namespace

Game& karubaTilesGame() {
  static KarubaTiles game;
  return game;
}

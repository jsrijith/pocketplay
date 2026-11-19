#include "pocketbgg.h"

#include <cstdio>
#include <vector>

#include "core/graphics.h"
#include "core/text.h"
#include "pocketbgg_list.h"
#include "pocketbgg_sample.h"
#include "ui/ui.h"

namespace bgg = pocketbgg;

namespace {

// The list: a caption line saying where the data came from and which page this is, then one
// row per game (the name, wrapped, and a line under it with the year and plays). The Up and
// Down buttons turn the pages, so the list runs to the bottom margin. The details: the same caption, a box where the thumbnail will go, the
// name large, and the year and plays.
constexpr int MARGIN = 16;
constexpr int GAP = 12;
constexpr int INNER_WIDTH = SCREEN_WIDTH - 2 * MARGIN;

constexpr int CAPTION_TOP = ui::CONTENT_TOP;
constexpr int CAPTION_HEIGHT = 24;
constexpr int LIST_TOP = CAPTION_TOP + CAPTION_HEIGHT + 8;

constexpr Font NAME_FONT = Font::Size20;
constexpr Font META_FONT = Font::Size14;
constexpr int ROW_INSET = 14;  // the text's distance from the row's outline, at the sides
constexpr int ROW_PAD_TOP = 6;
constexpr int ROW_PAD_BOTTOM = 8;
constexpr int NAME_WIDTH = INNER_WIDTH - 2 * ROW_INSET;
constexpr int META_HEIGHT = 26;

// The bottom row is left for actions to come, such as picking a random game. When one is added,
// end the list above it: LIST_BOTTOM = SCREEN_HEIGHT - MARGIN - ui::PILL_HEIGHT - GAP.
constexpr int LIST_BOTTOM = SCREEN_HEIGHT - MARGIN;
constexpr int LIST_ROOM = LIST_BOTTOM - LIST_TOP;

constexpr int THUMB = 216;  // BGG's thumbnails are up to 200 x 150, so this leaves a border
constexpr int THUMB_LEFT = (SCREEN_WIDTH - THUMB) / 2;
constexpr int THUMB_TOP = LIST_TOP;
constexpr int DETAIL_NAME_TOP = THUMB_TOP + THUMB + 2 * GAP;

int rowHeight(const bgg::Game& game) {
  return ROW_PAD_TOP + wrappedHeight(NAME_FONT, NAME_WIDTH, game.name) + META_HEIGHT + ROW_PAD_BOTTOM;
}

class PocketBgg final : public App {
 public:
  const char* name() const override { return "POCKETBGG"; }

  void open() override {
    if (!heights_.empty()) return;
    for (int i = 0; i < bgg::kSampleCount; i++) heights_.push_back(rowHeight(bgg::kSampleGames[i]));
    pages_ = bgg::paginate(heights_, LIST_ROOM, ui::ROW_GAP);
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) {
      if (selected_ < 0) return TapResult::Leave;
      selected_ = -1;
      return TapResult::RedrawFull;
    }
    if (selected_ >= 0) return TapResult::Nothing;

    for (int row = firstRow(); row < endRow(); row++) {
      if (ui::isInBox(x, y, MARGIN, rowTop(row), INNER_WIDTH, heights_[row])) {
        selected_ = row;
        return TapResult::RedrawFull;
      }
    }
    return TapResult::Nothing;
  }

  // Up and Down turn the list's pages, and do nothing past the first or last. On a game's
  // details they do nothing yet.
  TapResult onUpPressed() override { return selected_ < 0 ? turnPage(-1) : TapResult::Nothing; }
  TapResult onDownPressed() override { return selected_ < 0 ? turnPage(1) : TapResult::Nothing; }

  void draw(uint8_t* frame) override {
    ui::drawBand(frame, name(), nullptr, true);
    if (selected_ < 0) {
      drawList(frame);
    } else {
      drawDetails(frame, bgg::kSampleGames[selected_]);
    }
  }

  // Nothing is saved yet: the games are made up, so there's nothing worth keeping.
  bool saveFailed() const override { return false; }
  void retrySave() override {}

 private:
  int pageCount() const { return static_cast<int>(pages_.size()); }
  int firstRow() const { return pages_[page_]; }
  int endRow() const { return page_ + 1 < pageCount() ? pages_[page_ + 1] : bgg::kSampleCount; }

  // The rows stack from the top of the list, so a row's top is the sum of the ones above it on
  // its page. Drawing and tapping both ask this, so they can't drift apart.
  int rowTop(int row) const {
    int top = LIST_TOP;
    for (int r = firstRow(); r < row; r++) top += heights_[r] + ui::ROW_GAP;
    return top;
  }

  TapResult turnPage(int step) {
    const int next = page_ + step;
    if (next < 0 || next >= pageCount()) return TapResult::Nothing;
    page_ = next;
    return TapResult::Redraw;
  }

  // "SAMPLE DATA" on the left, so made-up games are never taken for a real collection, and
  // which page this is on the right.
  void drawCaption(uint8_t* frame, bool withPage) const {
    ui::drawTextLeft(frame, Font::Size14, MARGIN, CAPTION_TOP, CAPTION_HEIGHT, "SAMPLE DATA");
    if (!withPage) return;
    char page[24];
    std::snprintf(page, sizeof(page), "PAGE %d OF %d", page_ + 1, pageCount());
    const int width = textWidth(Font::Size14, page);
    ui::drawTextLeft(frame, Font::Size14, MARGIN + INNER_WIDTH - width, CAPTION_TOP, CAPTION_HEIGHT, page);
  }

  void drawList(uint8_t* frame) const {
    drawCaption(frame, true);
    for (int row = firstRow(); row < endRow(); row++) {
      const bgg::Game& game = bgg::kSampleGames[row];
      const int top = rowTop(row);
      drawRectOutline(frame, MARGIN, top, INNER_WIDTH, heights_[row], ui::HAIRLINE);
      const int nameHeight = drawWrapped(frame, NAME_FONT, MARGIN + ROW_INSET, top + ROW_PAD_TOP, NAME_WIDTH, game.name);
      char meta[40];
      bgg::describe(game, meta, sizeof(meta));
      ui::drawTextLeft(frame, META_FONT, MARGIN + ROW_INSET, top + ROW_PAD_TOP + nameHeight, META_HEIGHT, meta);
    }
  }

  void drawDetails(uint8_t* frame, const bgg::Game& game) const {
    drawCaption(frame, false);
    // Where the thumbnail goes once the app can download one (M6).
    ui::drawDimmedPill(frame, THUMB_LEFT, THUMB_TOP, THUMB, "", THUMB);
    drawTextInBox(frame, Font::Size14, THUMB_LEFT, THUMB_TOP, THUMB, THUMB, "NO IMAGE YET");

    // The name as large as fits in three lines, never cut short.
    Font font = Font::Size30;
    if (wrappedHeight(font, INNER_WIDTH, game.name) > 3 * lineHeight(font)) font = Font::Size20;
    const int nameHeight = drawWrapped(frame, font, MARGIN, DETAIL_NAME_TOP, INNER_WIDTH, game.name, Align::Center);

    char meta[40];
    bgg::describe(game, meta, sizeof(meta));
    const int metaTop = DETAIL_NAME_TOP + nameHeight + GAP;
    drawTextInBox(frame, Font::Size20, MARGIN, metaTop, INNER_WIDTH, lineHeight(Font::Size20), meta);

    char id[32];
    std::snprintf(id, sizeof(id), "BGG ID %u", static_cast<unsigned>(game.id));
    drawTextInBox(frame, Font::Size14, MARGIN, metaTop + lineHeight(Font::Size20) + 4, INNER_WIDTH, lineHeight(Font::Size14), id);
  }

  std::vector<int> heights_;  // each row's height, measured once
  std::vector<int> pages_;    // the first row of each page
  int page_ = 0;
  int selected_ = -1;  // the game whose details are showing, or -1 for the list
};

}  // namespace

App& pocketBggApp() {
  static PocketBgg app;
  return app;
}

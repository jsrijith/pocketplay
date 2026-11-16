#include "calculator.h"

#include <Arduino.h>

#include <cstring>

#include "calculator_icons.h"
#include "calculator_rules.h"
#include "calculator_save.h"
#include "core/graphics.h"
#include "core/storage.h"
#include "core/text.h"
#include "ui/ui.h"

namespace calc = calculator;

namespace {

constexpr const char* SAVE_FILE = "calculator";

// The display under the band, then a keypad of 5 rows by 4 columns anchored to the
// bottom margin, as other screens anchor their buttons.
constexpr int MARGIN = 16;
constexpr int GAP = 12;
constexpr int INNER_WIDTH = SCREEN_WIDTH - 2 * MARGIN;

constexpr int DISPLAY_TOP = ui::CONTENT_TOP;
constexpr int DISPLAY_HEIGHT = 148;
constexpr int DISPLAY_PADDING = 16;
// The sum waiting, in Size20, along the display's top.
constexpr int PENDING_TOP = DISPLAY_TOP + 8;
constexpr int PENDING_HEIGHT = 28;
// The number under it: Size64, whose capitals are 82 tall, with a clear gap above.
constexpr int NUMBER_TOP = PENDING_TOP + PENDING_HEIGHT + 4;
constexpr int NUMBER_HEIGHT = DISPLAY_TOP + DISPLAY_HEIGHT - 4 - NUMBER_TOP;

constexpr int COLUMNS = 4;
constexpr int ROWS = 5;
constexpr int KEY_WIDTH = (INNER_WIDTH - (COLUMNS - 1) * GAP) / COLUMNS;  // 103
constexpr int KEYPAD_BOTTOM = SCREEN_HEIGHT - MARGIN;
constexpr int KEY_HEIGHT = (KEYPAD_BOTTOM - (DISPLAY_TOP + DISPLAY_HEIGHT + GAP) - (ROWS - 1) * GAP) / ROWS;  // 93
constexpr int KEYPAD_TOP = KEYPAD_BOTTOM - ROWS * KEY_HEIGHT - (ROWS - 1) * GAP;
static_assert(KEYPAD_TOP >= DISPLAY_TOP + DISPLAY_HEIGHT + GAP, "the keypad must start below the display");

// The waiting operator's key gets a selection frame inside its outline.
constexpr int SELECTION_INSET = 6;

struct KeySpec {
  calc::Key key;
  const char* label;     // drawn in the font, or nullptr for an icon
  const uint8_t* icon;   // calculatorIcons::KEY square
  int column, row, span;
};

constexpr KeySpec KEYS[] = {
    {calc::Key::Clear, "C", nullptr, 0, 0, 1},
    {calc::Key::Backspace, nullptr, calculatorIcons::BACKSPACE, 1, 0, 1},
    {calc::Key::Percent, "%", nullptr, 2, 0, 1},
    {calc::Key::Divide, nullptr, calculatorIcons::DIVIDE, 3, 0, 1},
    {calc::Key::Digit7, "7", nullptr, 0, 1, 1},
    {calc::Key::Digit8, "8", nullptr, 1, 1, 1},
    {calc::Key::Digit9, "9", nullptr, 2, 1, 1},
    {calc::Key::Multiply, nullptr, calculatorIcons::TIMES, 3, 1, 1},
    {calc::Key::Digit4, "4", nullptr, 0, 2, 1},
    {calc::Key::Digit5, "5", nullptr, 1, 2, 1},
    {calc::Key::Digit6, "6", nullptr, 2, 2, 1},
    {calc::Key::Subtract, nullptr, calculatorIcons::MINUS, 3, 2, 1},
    {calc::Key::Digit1, "1", nullptr, 0, 3, 1},
    {calc::Key::Digit2, "2", nullptr, 1, 3, 1},
    {calc::Key::Digit3, "3", nullptr, 2, 3, 1},
    {calc::Key::Add, nullptr, calculatorIcons::PLUS, 3, 3, 1},
    {calc::Key::Digit0, "0", nullptr, 0, 4, 2},
    {calc::Key::Point, ".", nullptr, 2, 4, 1},
    {calc::Key::Equals, nullptr, calculatorIcons::EQUALS, 3, 4, 1},
};
constexpr int KEY_COUNT = sizeof(KEYS) / sizeof(KEYS[0]);

struct Box {
  int left, top, width, height;
};

// Where each key is, filled in from KEYS once and read by both draw() and onTap(), so
// what's drawn and what answers can't drift apart.
struct Layout {
  Box keys[KEY_COUNT];
};

Layout makeLayout() {
  Layout layout{};
  for (int i = 0; i < KEY_COUNT; i++) {
    const KeySpec& spec = KEYS[i];
    layout.keys[i] = {MARGIN + spec.column * (KEY_WIDTH + GAP), KEYPAD_TOP + spec.row * (KEY_HEIGHT + GAP),
                      spec.span * KEY_WIDTH + (spec.span - 1) * GAP, KEY_HEIGHT};
  }
  return layout;
}

const Layout LAYOUT = makeLayout();

calc::Key keyFor(calc::Op op) {
  switch (op) {
    case calc::Op::Add:
      return calc::Key::Add;
    case calc::Op::Subtract:
      return calc::Key::Subtract;
    case calc::Op::Multiply:
      return calc::Key::Multiply;
    default:
      return calc::Key::Divide;
  }
}

const uint8_t* smallIconFor(calc::Op op) {
  switch (op) {
    case calc::Op::Add:
      return calculatorIcons::SMALL_PLUS;
    case calc::Op::Subtract:
      return calculatorIcons::SMALL_MINUS;
    case calc::Op::Multiply:
      return calculatorIcons::SMALL_TIMES;
    default:
      return calculatorIcons::SMALL_DIVIDE;
  }
}

// What the screen shows of a state. A press that leaves this alone draws nothing,
// even if it changed something unseen, such as whether the next digit starts afresh.
bool looksSame(const calc::State& a, const calc::State& b) {
  if (a.error != b.error || a.pending != b.pending || strcmp(a.entry, b.entry) != 0) return false;
  return a.pending == calc::Op::None || a.accumulator == b.accumulator;
}

// The biggest cut the text fits in, never shortened (DESIGN.md).
Font numberFont(const char* text, int width) {
  if (textWidth(Font::Size64, text) <= width) return Font::Size64;
  if (textWidth(Font::Size44, text) <= width) return Font::Size44;
  return Font::Size30;
}

void drawRight(uint8_t* frame, Font font, int right, int top, int height, const char* text) {
  const int baseline = top + (height + capHeight(font)) / 2;
  drawText(frame, font, right, baseline, text, Align::Right);
}

void drawKey(uint8_t* frame, const KeySpec& spec, const Box& box, bool selected) {
  const bool filled = spec.key == calc::Key::Equals;  // the one main action
  ui::drawPill(frame, box.left, box.top, box.width, "", filled, box.height);
  if (spec.label != nullptr) {
    drawTextInBox(frame, Font::Size44, box.left, box.top, box.width, box.height, spec.label, !filled);
  } else {
    drawIcon(frame, spec.icon, calculatorIcons::KEY, calculatorIcons::KEY, box.left + (box.width - calculatorIcons::KEY) / 2,
             box.top + (box.height - calculatorIcons::KEY) / 2, !filled);
  }
  if (selected) {
    drawRoundRectOutline(frame, box.left + SELECTION_INSET, box.top + SELECTION_INSET, box.width - 2 * SELECTION_INSET,
                         box.height - 2 * SELECTION_INSET, ui::PILL_RADIUS - SELECTION_INSET, ui::FRAME);
  }
}

class Calculator final : public App {
 public:
  const char* name() const override { return "CALCULATOR"; }
  const uint8_t* icon() const override { return calculatorIcons::APP; }

  void open() override {
    if (loaded_) return;
    loaded_ = true;
    load();
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) return TapResult::Leave;
    for (int i = 0; i < KEY_COUNT; i++) {
      const Box& box = LAYOUT.keys[i];
      if (ui::isInBox(x, y, box.left, box.top, box.width, box.height)) return pressKey(KEYS[i].key);
    }
    return TapResult::Nothing;
  }

  void draw(uint8_t* frame) override {
    ui::drawBand(frame, name(), nullptr, true);
    drawDisplay(frame);
    const bool waiting = !state_.error && state_.pending != calc::Op::None;
    for (int i = 0; i < KEY_COUNT; i++) {
      drawKey(frame, KEYS[i], LAYOUT.keys[i], waiting && KEYS[i].key == keyFor(state_.pending));
    }
  }

  bool saveFailed() const override { return saveFailed_; }
  void retrySave() override { persist(); }

 private:
  TapResult pressKey(calc::Key key) {
    const calc::State before = state_;
    if (!calc::press(state_, key)) return TapResult::Nothing;
    persist();
    return looksSame(before, state_) ? TapResult::Nothing : TapResult::Redraw;
  }

  void drawDisplay(uint8_t* frame) {
    drawRectOutline(frame, MARGIN, DISPLAY_TOP, INNER_WIDTH, DISPLAY_HEIGHT, 2);
    const int right = MARGIN + INNER_WIDTH - DISPLAY_PADDING;
    const int width = INNER_WIDTH - 2 * DISPLAY_PADDING;

    // The sum waiting, such as "12 +", so the player can see what = will do.
    if (!state_.error && state_.pending != calc::Op::None) {
      const int iconLeft = right - calculatorIcons::SMALL;
      drawIcon(frame, smallIconFor(state_.pending), calculatorIcons::SMALL, calculatorIcons::SMALL, iconLeft,
               PENDING_TOP + (PENDING_HEIGHT - calculatorIcons::SMALL) / 2);
      char first[calc::MAX_ENTRY + 1];
      if (calc::format(state_.accumulator, first, sizeof(first))) {
        drawRight(frame, Font::Size20, iconLeft - 6, PENDING_TOP, PENDING_HEIGHT, first);
      }
    }

    const char* number = state_.error ? "ERROR" : state_.entry;
    drawRight(frame, numberFont(number, width), right, NUMBER_TOP, NUMBER_HEIGHT, number);
  }

  void load() {
    if (!storage::ready()) return;
    uint8_t data[calc::SAVE_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (storage::load(SAVE_FILE, version, data, sizeof(data), size) && !calc::unpack(version, data, size, state_)) {
      Serial.println("Ignoring saved calculator state");
    }
  }

  // Every change, so a wake shows the same number.
  void persist() {
    if (!storage::ready()) {
      saveFailed_ = false;
      return;
    }
    uint8_t data[calc::SAVE_BYTES];
    const size_t size = calc::pack(state_, data);
    saveFailed_ = !storage::save(SAVE_FILE, calc::SAVE_VERSION, data, size);
  }

  calc::State state_{};
  bool loaded_ = false;
  bool saveFailed_ = false;
};

}  // namespace

App& calculatorApp() {
  static Calculator app;
  return app;
}

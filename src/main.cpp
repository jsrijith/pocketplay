// BoardByte: shows the Sticky's three buttons on screen, with a press count for each.
//
// Boot order matters on this board: power latches, then the SD rail, then the
// panel. Waking from deep sleep is a full chip reset, so every wake runs setup()
// again from the top, and the press counts start over.

#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <PowerManager.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include <cstdio>
#include <cstring>

#include "font.h"

namespace {

constexpr uint32_t POLL_MS = 5;
constexpr uint32_t DEBOUNCE_MS = 20;
constexpr uint32_t SLEEP_HOLD_MS = 1000;
constexpr uint32_t IDLE_SLEEP_MS = 10UL * 60UL * 1000UL;

// Fast refreshes slowly leave ghosts behind; a full refresh clears them.
constexpr int FAST_REFRESHES_PER_FULL = 30;

constexpr uint16_t COUNT_LIMIT = 10000;  // wraps to 0, so a count never exceeds 4 digits

constexpr int SCREEN_WIDTH = EInkDisplay::DISPLAY_WIDTH;
constexpr int SCREEN_HEIGHT = EInkDisplay::DISPLAY_HEIGHT;

constexpr int BOX_GAP = 16;
constexpr int BOX_MARGIN = 24;
constexpr int BOX_WIDTH = (SCREEN_WIDTH - 2 * BOX_MARGIN - 2 * BOX_GAP) / 3;
constexpr int BOX_HEIGHT = 320;
constexpr int BOX_TOP = (SCREEN_HEIGHT - BOX_HEIGHT) / 2;
constexpr int BOX_BORDER = 4;
constexpr int LABEL_SCALE = 6;
constexpr int LABEL_OFFSET_Y = 70;
constexpr int COUNT_SCALE = 8;
constexpr int COUNT_OFFSET_Y = 190;
constexpr int ASLEEP_SCALE = 10;

struct Button {
  const char* label;
  int8_t pin;
  bool rawDown;           // the latest reading, which may still be bouncing
  bool down;              // the debounced state
  uint32_t rawChangedMs;  // when the raw reading last changed
  uint32_t pressStartMs;  // when the debounced state last went down
  uint16_t count;
};

// In on-screen order, left to right. Pins are filled in by beginButtons().
Button buttons[] = {
    {"UP", BoardConfig::PIN_UNASSIGNED, false, false, 0, 0, 0},
    {"OK", BoardConfig::PIN_UNASSIGNED, false, false, 0, 0, 0},
    {"DOWN", BoardConfig::PIN_UNASSIGNED, false, false, 0, 0, 0},
};
Button& okButton = buttons[1];

// The SDK's begin() takes its wiring from BoardConfig; these arguments are unused.
EInkDisplay display(BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED);

bool ready = false;
bool screenDirty = false;
bool releasedSinceBoot = false;
int fastRefreshes = 0;
uint32_t lastActivityMs = 0;

// Framebuffer format: 1 bit per pixel, most significant bit first, 1 = white.
void setPixel(uint8_t* frame, int x, int y, bool black) {
  uint8_t& byte = frame[y * EInkDisplay::DISPLAY_WIDTH_BYTES + x / 8];
  const auto mask = static_cast<uint8_t>(0x80 >> (x % 8));
  if (black) {
    byte &= static_cast<uint8_t>(~mask);
  } else {
    byte |= mask;
  }
}

void fillRect(uint8_t* frame, int left, int top, int width, int height, bool black) {
  for (int y = top; y < top + height; y++) {
    for (int x = left; x < left + width; x++) {
      setPixel(frame, x, y, black);
    }
  }
}

void drawRectOutline(uint8_t* frame, int left, int top, int width, int height, int thickness) {
  fillRect(frame, left, top, width, thickness, true);
  fillRect(frame, left, top + height - thickness, width, thickness, true);
  fillRect(frame, left, top, thickness, height, true);
  fillRect(frame, left + width - thickness, top, thickness, height, true);
}

void drawGlyph(uint8_t* frame, const font::Glyph& glyph, int left, int top, int scale, bool black) {
  for (int row = 0; row < font::GLYPH_HEIGHT; row++) {
    for (int col = 0; col < font::GLYPH_WIDTH; col++) {
      if (!(glyph.rows[row] & (1 << (font::GLYPH_WIDTH - 1 - col)))) continue;
      fillRect(frame, left + col * scale, top + row * scale, scale, scale, black);
    }
  }
}

// Draws text with its horizontal centre at centerX.
void drawText(uint8_t* frame, const char* text, int centerX, int top, int scale, bool black) {
  const int advance = (font::GLYPH_WIDTH + 1) * scale;
  const int characters = static_cast<int>(strlen(text));
  const int width = characters * advance - scale;  // no gap after the last character
  const int left = centerX - width / 2;

  for (int i = 0; i < characters; i++) {
    if (const font::Glyph* glyph = font::find(text[i])) {
      drawGlyph(frame, *glyph, left + i * advance, top, scale, black);
    }
  }
}

void drawButtonBox(uint8_t* frame, const Button& button, int left) {
  if (button.down) {
    fillRect(frame, left, BOX_TOP, BOX_WIDTH, BOX_HEIGHT, true);
  } else {
    drawRectOutline(frame, left, BOX_TOP, BOX_WIDTH, BOX_HEIGHT, BOX_BORDER);
  }

  const int centerX = left + BOX_WIDTH / 2;
  const bool textBlack = !button.down;  // white text on a held button's black box
  drawText(frame, button.label, centerX, BOX_TOP + LABEL_OFFSET_Y, LABEL_SCALE, textBlack);

  char count[6];
  snprintf(count, sizeof(count), "%u", button.count);
  drawText(frame, count, centerX, BOX_TOP + COUNT_OFFSET_Y, COUNT_SCALE, textBlack);
}

void drawScreen() {
  display.clearScreen(0xFF);
  uint8_t* frame = display.getFrameBuffer();
  int left = BOX_MARGIN;
  for (const Button& button : buttons) {
    drawButtonBox(frame, button, left);
    left += BOX_WIDTH + BOX_GAP;
  }
}

void beginButtons() {
  const auto& input = BoardConfig::ACTIVE.input;
  buttons[0].pin = input.up;
  buttons[1].pin = input.power;  // OK and power share one pin on the Sticky
  buttons[2].pin = input.down;

  // Start from the real pin state, so the press that woke the device isn't counted.
  for (Button& button : buttons) {
    pinMode(button.pin, INPUT_PULLUP);
    button.rawDown = digitalRead(button.pin) == LOW;
    button.down = button.rawDown;
  }
}

// Returns true if any button's debounced state changed.
bool updateButtons(uint32_t now) {
  bool changed = false;
  for (Button& button : buttons) {
    const bool raw = digitalRead(button.pin) == LOW;
    if (raw != button.rawDown) {
      button.rawDown = raw;
      button.rawChangedMs = now;
    }
    if (raw == button.down || now - button.rawChangedMs < DEBOUNCE_MS) continue;

    button.down = raw;
    changed = true;
    if (raw) {
      button.pressStartMs = now;
      button.count = (button.count + 1) % COUNT_LIMIT;
      Serial.printf("%s pressed, count %u\n", button.label, button.count);
    }
  }
  return changed;
}

// Returns as soon as the frame is sent, so buttons keep being read while the
// panel updates itself. The framebuffer is free to redraw straight away.
void startRefresh() {
  if (++fastRefreshes >= FAST_REFRESHES_PER_FULL) {
    fastRefreshes = 0;
    display.displayBufferAsync(EInkDisplay::FULL_REFRESH);
  } else {
    display.displayBufferAsync(EInkDisplay::FAST_REFRESH);
  }
}

void showAsleep() {
  display.clearScreen(0xFF);
  drawText(display.getFrameBuffer(), "ASLEEP", SCREEN_WIDTH / 2, (SCREEN_HEIGHT - font::GLYPH_HEIGHT * ASLEEP_SCALE) / 2, ASLEEP_SCALE, true);

  // Async even though it waits: after an async refresh the panel's stored
  // previous frame is out of date, and only this path sends the correct one.
  display.displayBufferAsync(EInkDisplay::FAST_REFRESH);
  display.waitRefreshComplete();
}

[[noreturn]] void goToSleep() {
  Serial.println("Going to sleep. Press OK to wake.");
  Serial.flush();

  // E-ink keeps its last image, so don't leave the OK box black during sleep.
  showAsleep();

  // Panel first, while its power rail is still on.
  display.deepSleep();

  // Deep sleep isolates the pads, so the latches need a hold or they would float.
  for (const int8_t pin : {BoardConfig::ACTIVE.power.latch0, BoardConfig::ACTIVE.power.latch1}) {
    const auto gpio = static_cast<gpio_num_t>(pin);
    gpio_hold_dis(gpio);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    gpio_hold_en(gpio);
  }

  freeink::PowerManager::powerDownRailsForSleep();
  freeink::PowerManager::deepSleepUntilPowerButton();
}

}  // namespace

void setup() {
  // Must stay first: on battery, the board relies on these once OK is released.
  BoardConfig::holdPowerRails();

  Serial.begin(115200);
  Serial.printf("\nBoardByte starting, wake cause %d\n", static_cast<int>(esp_sleep_get_wakeup_cause()));

  // The SD card shares the panel's SPI bus. Unpowered, it clamps the bus and the
  // panel never hears a command. An earlier firmware's off-hold can survive
  // reflashing, so this also clears that.
  BoardConfig::releaseSdRail();

  beginButtons();

  display.begin();
  if (!display.framebufferReady()) {
    Serial.println("ERROR: no memory for the framebuffer");
    return;
  }

  // The first paint after a boot must be a full refresh to clear the old image.
  drawScreen();
  display.displayBufferAsync(EInkDisplay::FULL_REFRESH);
  Serial.println("Button test is on the screen.");

  lastActivityMs = millis();
  ready = true;
}

void loop() {
  if (!ready) {
    // Setup failed. Stay awake so the error can still be read over serial.
    delay(1000);
    return;
  }

  const uint32_t now = millis();

  if (updateButtons(now)) {
    screenDirty = true;
    lastActivityMs = now;
  }

  // Changes made during a refresh are combined into the next frame.
  if (screenDirty && !display.refreshBusy()) {
    screenDirty = false;
    drawScreen();
    startRefresh();
  }

  // The press that woke the device must end before a hold can mean "sleep".
  if (!okButton.down) releasedSinceBoot = true;

  if (releasedSinceBoot && okButton.down && now - okButton.pressStartMs >= SLEEP_HOLD_MS) goToSleep();
  if (now - lastActivityMs >= IDLE_SLEEP_MS) goToSleep();

  delay(POLL_MS);
}

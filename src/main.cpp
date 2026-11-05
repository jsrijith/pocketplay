// BoardByte: shows the Sticky's buttons and touch input on screen.
//
// Boot order matters on this board: power latches, then the SD rail, then input
// and the panel. Waking from deep sleep is a full chip reset, so every wake runs
// setup() again from the top, and all the counts start over.

#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <PowerManager.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "font.h"

namespace {

// InputManager doesn't debounce buttons on this board. Sampling further apart
// than a button bounces means one press can only produce one edge.
constexpr uint32_t POLL_MS = 15;
constexpr uint32_t SLEEP_HOLD_MS = 1000;
constexpr uint32_t IDLE_SLEEP_MS = 10UL * 60UL * 1000UL;

// Fast refreshes slowly leave ghosts behind; a full refresh clears them.
constexpr int FAST_REFRESHES_PER_FULL = 30;

constexpr uint16_t COUNT_LIMIT = 10000;  // wraps to 0, so a count never exceeds 4 digits

// The panel is natively landscape. The firmware draws in portrait, held with the
// USB edge on the right. setPixel() and the touch helpers convert between the two.
constexpr int SCREEN_WIDTH = EInkDisplay::DISPLAY_HEIGHT;
constexpr int SCREEN_HEIGHT = EInkDisplay::DISPLAY_WIDTH;

constexpr int TEXT_SCALE = 4;

constexpr int BOX_GAP = 12;
constexpr int BOX_MARGIN = 12;
constexpr int BOX_WIDTH = (SCREEN_WIDTH - 2 * BOX_MARGIN - 2 * BOX_GAP) / 3;
constexpr int BOX_HEIGHT = 130;
constexpr int BOX_TOP = 24;
constexpr int BOX_BORDER = 4;
constexpr int BOX_LABEL_OFFSET = 24;
constexpr int BOX_COUNT_OFFSET = 74;

constexpr int STATUS_TOP = SCREEN_HEIGHT - 170;
constexpr int STATUS_LINE_HEIGHT = 50;

constexpr int CROSS_ARM = 24;
constexpr int CROSS_THICKNESS = 4;

constexpr int ASLEEP_SCALE = 10;

struct Button {
  const char* label;
  uint8_t index;  // InputManager's BTN_* value
  uint16_t count;
};

// In on-screen order, left to right. OK is read as BTN_POWER: setup() turns on the
// setting that makes it follow the button straight away, so it can count on press.
Button buttons[] = {
    {"UP", InputManager::BTN_UP, 0},
    {"OK", InputManager::BTN_POWER, 0},
    {"DOWN", InputManager::BTN_DOWN, 0},
};

InputManager input;

// The SDK's begin() takes its wiring from BoardConfig; these arguments are unused.
EInkDisplay display(BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED);

bool ready = false;
bool screenDirty = false;
bool releasedSinceBoot = false;
int fastRefreshes = 0;
uint32_t lastActivityMs = 0;

bool hasTapped = false;
int tapX = 0;
int tapY = 0;
uint16_t tapCount = 0;
uint16_t swipeCount = 0;
const char* swipeDirection = "";

// Framebuffer format: 1 bit per pixel, most significant bit first, 1 = white.
// Touch can put the cross at a screen edge, so anything outside the screen is skipped.
void setPixel(uint8_t* frame, int x, int y, bool black) {
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;

  const int panelX = y;
  const int panelY = SCREEN_WIDTH - 1 - x;
  uint8_t& byte = frame[panelY * EInkDisplay::DISPLAY_WIDTH_BYTES + panelX / 8];
  const auto mask = static_cast<uint8_t>(0x80 >> (panelX % 8));
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

// Draws text with its horizontal centre at centerX. Spaces have no glyph, so they
// just leave a gap.
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
  const bool held = input.isPressed(button.index);
  if (held) {
    fillRect(frame, left, BOX_TOP, BOX_WIDTH, BOX_HEIGHT, true);
  } else {
    drawRectOutline(frame, left, BOX_TOP, BOX_WIDTH, BOX_HEIGHT, BOX_BORDER);
  }

  const int centerX = left + BOX_WIDTH / 2;
  const bool textBlack = !held;  // white text on a held button's black box
  drawText(frame, button.label, centerX, BOX_TOP + BOX_LABEL_OFFSET, TEXT_SCALE, textBlack);

  char count[6];
  snprintf(count, sizeof(count), "%u", button.count);
  drawText(frame, count, centerX, BOX_TOP + BOX_COUNT_OFFSET, TEXT_SCALE, textBlack);
}

void drawCross(uint8_t* frame, int x, int y) {
  fillRect(frame, x - CROSS_ARM, y - CROSS_THICKNESS / 2, CROSS_ARM * 2, CROSS_THICKNESS, true);
  fillRect(frame, x - CROSS_THICKNESS / 2, y - CROSS_ARM, CROSS_THICKNESS, CROSS_ARM * 2, true);
}

void drawTouchStatus(uint8_t* frame) {
  const int centerX = SCREEN_WIDTH / 2;

  if (!input.hasTouch()) {
    drawText(frame, "NO TOUCH", centerX, STATUS_TOP, TEXT_SCALE, true);
    return;
  }

  char text[32];
  if (hasTapped) {
    snprintf(text, sizeof(text), "X %d Y %d", tapX, tapY);
    drawText(frame, text, centerX, STATUS_TOP, TEXT_SCALE, true);
  }

  snprintf(text, sizeof(text), "TAPS %u", tapCount);
  drawText(frame, text, centerX, STATUS_TOP + STATUS_LINE_HEIGHT, TEXT_SCALE, true);

  if (swipeCount > 0) {
    snprintf(text, sizeof(text), "SWIPE %s %u", swipeDirection, swipeCount);
  } else {
    snprintf(text, sizeof(text), "SWIPE %u", swipeCount);
  }
  drawText(frame, text, centerX, STATUS_TOP + 2 * STATUS_LINE_HEIGHT, TEXT_SCALE, true);
}

void drawScreen() {
  display.clearScreen(0xFF);
  uint8_t* frame = display.getFrameBuffer();

  int left = BOX_MARGIN;
  for (const Button& button : buttons) {
    drawButtonBox(frame, button, left);
    left += BOX_WIDTH + BOX_GAP;
  }

  drawTouchStatus(frame);
  if (hasTapped) drawCross(frame, tapX, tapY);
}

// Returns true if anything on screen needs to change.
bool handleButtons() {
  for (Button& button : buttons) {
    if (!input.wasPressed(button.index)) continue;
    button.count = (button.count + 1) % COUNT_LIMIT;
    Serial.printf("%s pressed, count %u\n", button.label, button.count);
  }
  return input.wasAnyPressed() || input.wasAnyReleased();
}

// Touch positions arrive as 0..1 in the panel's landscape frame.
int touchToScreenX(float panelY) { return static_cast<int>((1.0f - panelY) * (SCREEN_WIDTH - 1)); }
int touchToScreenY(float panelX) { return static_cast<int>(panelX * (SCREEN_HEIGHT - 1)); }

// Returns true if anything on screen needs to change. A swipe and a tap can't
// both fire for one touch: a tap has to move less than a swipe's minimum distance.
bool handleTouch() {
  float startX, startY, endX, endY;
  if (input.wasSwipe(startX, startY, endX, endY)) {
    const int dx = touchToScreenX(endY) - touchToScreenX(startY);
    const int dy = touchToScreenY(endX) - touchToScreenY(startX);
    if (abs(dx) >= abs(dy)) {
      swipeDirection = dx < 0 ? "LEFT" : "RIGHT";
    } else {
      swipeDirection = dy < 0 ? "UP" : "DOWN";
    }
    swipeCount = (swipeCount + 1) % COUNT_LIMIT;
    Serial.printf("SWIPE %s\n", swipeDirection);
    return true;
  }

  float x, y;
  if (input.wasTouchTap(x, y)) {
    tapX = touchToScreenX(y);
    tapY = touchToScreenY(x);
    hasTapped = true;
    tapCount = (tapCount + 1) % COUNT_LIMIT;
    Serial.printf("TAP %d %d\n", tapX, tapY);
    return true;
  }

  return false;
}

// Returns as soon as the frame is sent, so input keeps being read while the
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

  InputManager::setSharedConfirmPowerShortPressEmitsPower(true);
  input.begin();
  if (!input.hasTouch()) Serial.println("ERROR: the touch controller did not answer");

  // Absorbs any button already held, so the press that woke the device isn't
  // counted, but its box still draws black.
  input.update();

  display.begin();
  if (!display.framebufferReady()) {
    Serial.println("ERROR: no memory for the framebuffer");
    return;
  }

  // The first paint after a boot must be a full refresh to clear the old image.
  drawScreen();
  display.displayBufferAsync(EInkDisplay::FULL_REFRESH);
  Serial.println("Input test is on the screen.");

  lastActivityMs = millis();
  ready = true;
}

void loop() {
  if (!ready) {
    // Setup failed. Stay awake so the error can still be read over serial.
    delay(1000);
    return;
  }

  input.update();
  const uint32_t now = millis();

  const bool buttonsChanged = handleButtons();
  const bool touchChanged = handleTouch();
  if (buttonsChanged || touchChanged) screenDirty = true;
  if (buttonsChanged || input.wasTouchActivity()) lastActivityMs = now;

  // Changes made during a refresh are combined into the next frame.
  if (screenDirty && !display.refreshBusy()) {
    screenDirty = false;
    drawScreen();
    startRefresh();
  }

  // The press that woke the device must end before a hold can mean "sleep".
  const bool okDown = input.isPressed(InputManager::BTN_POWER);
  if (!okDown) releasedSinceBoot = true;

  if (releasedSinceBoot && okDown && input.getPowerButtonHeldTime() >= SLEEP_HOLD_MS) goToSleep();
  if (now - lastActivityMs >= IDLE_SLEEP_MS) goToSleep();

  delay(POLL_MS);
}

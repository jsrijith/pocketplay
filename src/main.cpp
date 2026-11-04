// BoardByte v0: draw "HELLO WORLD" on the Seeed reTerminal Sticky.
//
// Boot order matters on this board: power latches, then the SD rail, then the
// panel. Waking from deep sleep is a full chip reset, so every wake runs setup()
// again from the top.

#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <PowerManager.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "font.h"

namespace {

constexpr char MESSAGE[] = "HELLO WORLD";
constexpr int TEXT_SCALE = 10;  // each font pixel becomes a 10x10 block

constexpr uint32_t SLEEP_HOLD_MS = 400;
constexpr uint32_t IDLE_SLEEP_MS = 10UL * 60UL * 1000UL;

// The SDK's begin() takes its wiring from BoardConfig; these arguments are unused.
EInkDisplay display(BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED);

int8_t okButton = BoardConfig::PIN_UNASSIGNED;
uint32_t lastActivityMs = 0;
uint32_t pressStartMs = 0;
bool pressed = false;
bool releasedSinceBoot = false;

// Framebuffer format: 1 bit per pixel, most significant bit first, 1 = white.
void setBlackPixel(uint8_t* frame, int x, int y) {
  frame[y * EInkDisplay::DISPLAY_WIDTH_BYTES + x / 8] &= static_cast<uint8_t>(~(0x80 >> (x % 8)));
}

void drawGlyph(uint8_t* frame, const font::Glyph& glyph, int left, int top) {
  for (int row = 0; row < font::GLYPH_HEIGHT; row++) {
    for (int col = 0; col < font::GLYPH_WIDTH; col++) {
      if (!(glyph.rows[row] & (1 << (font::GLYPH_WIDTH - 1 - col)))) continue;
      for (int dy = 0; dy < TEXT_SCALE; dy++) {
        for (int dx = 0; dx < TEXT_SCALE; dx++) {
          setBlackPixel(frame, left + col * TEXT_SCALE + dx, top + row * TEXT_SCALE + dy);
        }
      }
    }
  }
}

void drawMessage(uint8_t* frame) {
  constexpr int CHARACTERS = sizeof(MESSAGE) - 1;
  constexpr int ADVANCE = (font::GLYPH_WIDTH + 1) * TEXT_SCALE;
  constexpr int TEXT_WIDTH = CHARACTERS * ADVANCE - TEXT_SCALE;  // no gap after the last letter
  constexpr int TEXT_HEIGHT = font::GLYPH_HEIGHT * TEXT_SCALE;

  const int left = (EInkDisplay::DISPLAY_WIDTH - TEXT_WIDTH) / 2;
  const int top = (EInkDisplay::DISPLAY_HEIGHT - TEXT_HEIGHT) / 2;

  for (int i = 0; i < CHARACTERS; i++) {
    if (const font::Glyph* glyph = font::find(MESSAGE[i])) {
      drawGlyph(frame, *glyph, left + i * ADVANCE, top);
    }
  }
}

[[noreturn]] void goToSleep() {
  Serial.println("Going to sleep. Press OK to wake.");
  Serial.flush();

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

  display.begin();
  if (!display.framebufferReady()) {
    Serial.println("ERROR: no memory for the framebuffer");
    return;
  }

  display.clearScreen(0xFF);
  drawMessage(display.getFrameBuffer());
  display.displayBuffer(EInkDisplay::FULL_REFRESH);
  Serial.println("Hello World is on the screen.");

  okButton = BoardConfig::ACTIVE.input.power;
  pinMode(okButton, INPUT_PULLUP);
  lastActivityMs = millis();
}

void loop() {
  if (okButton == BoardConfig::PIN_UNASSIGNED) {
    // Setup failed. Stay awake so the error can still be read over serial.
    delay(1000);
    return;
  }

  const bool isDown = digitalRead(okButton) == LOW;
  const uint32_t now = millis();

  if (isDown && !pressed) {
    pressStartMs = now;
    lastActivityMs = now;
  } else if (!isDown && pressed) {
    lastActivityMs = now;
  }
  pressed = isDown;

  // The press that woke the device must end before a hold can mean "sleep".
  if (!isDown) releasedSinceBoot = true;

  if (releasedSinceBoot && isDown && now - pressStartMs >= SLEEP_HOLD_MS) goToSleep();
  if (now - lastActivityMs >= IDLE_SLEEP_MS) goToSleep();

  delay(10);
}

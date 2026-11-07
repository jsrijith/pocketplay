// PocketPlay: a games firmware for the Seeed reTerminal Sticky. This file runs the
// hardware: boot, input, the panel and sleep. shell/shell.cpp decides what's on screen.
//
// Boot order matters on this board: power latches, then the SD rail, then input,
// the card and the panel. Waking from deep sleep is a full chip reset, so every wake
// runs setup() again from the top, and anything worth keeping is read back from the card.

#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <PowerManager.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "core/clock.h"
#include "core/graphics.h"
#include "core/storage.h"
#include "core/version.h"
#include "shell/shell.h"
#include "ui/logo.h"

namespace {

// InputManager doesn't debounce buttons on this board. Sampling further apart
// than a button bounces means one press can only produce one edge.
constexpr uint32_t POLL_MS = 15;
constexpr uint32_t SLEEP_HOLD_MS = 1000;

// How long a finger rests on the screen before it's a hold, for a game that uses
// holds. CrossPlay's Minesweeper plants a flag at 400 ms: long enough that a
// deliberate tap never trips it, short enough not to feel broken on this panel.
constexpr unsigned long HOLD_MS = 400;

// How long the boot screen stays once it's on the panel, so it can be read.
constexpr uint32_t SPLASH_MS = 1500;

InputManager input;

// The SDK's begin() takes its wiring from BoardConfig; these arguments are unused.
EInkDisplay display(BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED);

bool ready = false;
bool releasedSinceBoot = false;
int fastRefreshes = 0;
// A press sent to the shell and not yet released, and whether it has become a hold.
bool pressOpen = false;
bool holdFired = false;
uint32_t lastActivityMs = 0;

// What the next refresh must be. Changes made during a refresh are combined into the
// next frame, and a full refresh asked for meanwhile isn't downgraded.
shell::Redraw pendingRedraw = shell::Redraw::None;

void requestRedraw(shell::Redraw redraw) {
  if (redraw == shell::Redraw::Full || pendingRedraw == shell::Redraw::None) {
    if (redraw != shell::Redraw::None) pendingRedraw = redraw;
  }
}

void drawScreen() {
  const uint32_t startUs = micros();

  display.clearScreen(0xFF);
  shell::draw(display.getFrameBuffer());

  Serial.printf("Screen drawn in %lu us\n", static_cast<unsigned long>(micros() - startUs));
}

// Touch positions arrive as 0..1 in the panel's landscape frame. A landscape game
// is drawn in that frame, so its taps pass straight through.
int touchToScreenX(float panelX, float panelY) {
  if (shell::landscape()) return static_cast<int>(panelX * (LANDSCAPE_WIDTH - 1));
  return static_cast<int>((1.0f - panelY) * (SCREEN_WIDTH - 1));
}
int touchToScreenY(float panelX, float panelY) {
  if (shell::landscape()) return static_cast<int>(panelY * (LANDSCAPE_HEIGHT - 1));
  return static_cast<int>(panelX * (SCREEN_HEIGHT - 1));
}

// Press, hold and release, for a game that uses holds. A press stays open while
// the SDK still counts it as a tap: down, and not moved past its tap slop.
void handleHold() {
  float x, y;
  if (input.wasTouchPressedAt(x, y)) {
    pressOpen = true;
    holdFired = false;
    requestRedraw(shell::onPress(touchToScreenX(x, y), touchToScreenY(x, y)));
  }
  if (!pressOpen) return;

  unsigned long heldMs = 0;
  if (input.isTouchTapCandidate(x, y, heldMs)) {
    if (holdFired || heldMs < HOLD_MS) return;
    holdFired = true;
    requestRedraw(shell::onHold(touchToScreenX(x, y), touchToScreenY(x, y)));
    // Always, even when the game did nothing with the hold, so the lift can't also
    // tap. CrossPlay found a finger that rolls between two cells can otherwise
    // hold one and tap the other.
    input.suppressTouchContact();
  }
  // Lifted, moved off, or swallowed by the hold just now.
  pressOpen = false;
  requestRedraw(shell::onRelease());
}

void handleInput() {
  if (input.wasPressed(InputManager::BTN_UP)) requestRedraw(shell::onUpPressed());
  if (input.wasPressed(InputManager::BTN_DOWN)) requestRedraw(shell::onDownPressed());

  float x, y;
  if (shell::usesHold()) handleHold();
  if (input.wasTouchTap(x, y)) requestRedraw(shell::onTap(touchToScreenX(x, y), touchToScreenY(x, y)));

  // The SDK never reports one contact as both a tap and a swipe.
  float endX, endY;
  if (input.wasSwipe(x, y, endX, endY)) {
    requestRedraw(shell::onSwipe(touchToScreenX(x, y), touchToScreenY(x, y), touchToScreenX(endX, endY), touchToScreenY(endX, endY)));
  }
}

// Returns as soon as the frame is sent, so input keeps being read while the
// panel updates itself. The framebuffer is free to redraw straight away.
void startRefresh(shell::Redraw redraw) {
  if (redraw == shell::Redraw::Full) {
    fastRefreshes = 0;
    display.displayBufferAsync(EInkDisplay::FULL_REFRESH);
    return;
  }

  // Fast refreshes slowly leave ghosts behind. A half refresh clears them, and is
  // gentler to watch than a full one.
  const int updatesPerCleanup = shell::updatesPerCleanup();
  if (updatesPerCleanup != 0 && ++fastRefreshes >= updatesPerCleanup) {
    fastRefreshes = 0;
    display.displayBufferAsync(EInkDisplay::HALF_REFRESH);
  } else {
    display.displayBufferAsync(EInkDisplay::FAST_REFRESH);
  }
}

// The logo, the name, `line` and the version, in portrait whatever was open.
void drawSplash(const char* line) {
  display.clearScreen(0xFF);
  setOrientation(Orientation::Portrait);
  ui::drawSplash(display.getFrameBuffer(), line);
}

void showAsleep() {
  // Always portrait, even from a landscape game: a wake starts there.
  drawSplash("PRESS OK TO WAKE");

  // Async even though it waits: after an async refresh the panel's stored
  // previous frame is out of date, and only this path sends the correct one. A full
  // refresh, because this image stays on the panel for as long as the device sleeps.
  display.displayBufferAsync(EInkDisplay::FULL_REFRESH);
  display.waitRefreshComplete();
}

[[noreturn]] void goToSleep() {
  shell::beforeSleep();

  Serial.println("Going to sleep. Press OK to wake.");
  Serial.flush();

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
  Serial.printf("\nPocketPlay starting, wake cause %d\n", static_cast<int>(esp_sleep_get_wakeup_cause()));

  // The SD card shares the panel's SPI bus. Unpowered, it clamps the bus and the
  // panel never hears a command. An earlier firmware's off-hold can survive
  // reflashing, so this also clears that.
  BoardConfig::releaseSdRail();

  InputManager::setSharedConfirmPowerShortPressEmitsPower(true);
  input.begin();
  if (!input.hasTouch()) Serial.println("ERROR: the touch controller did not answer");

  // Absorbs any button already held, so the press that woke the device does nothing.
  input.update();

  wallClock::begin();
  storage::begin();
  shell::begin(input.hasTouch());

  display.begin();
  if (!display.framebufferReady()) {
    Serial.println("ERROR: no memory for the framebuffer");
    return;
  }

  // The boot screen first, then the screen the shell has open. Both are full refreshes:
  // the first paint after a boot must clear the old image, and the second clears the logo.
  Serial.printf("PocketPlay %s, build %d (%s)\n", version::NUMBER, version::BUILD, version::COMMIT);
  drawSplash("GAMES IN YOUR POCKET");
  display.displayBufferAsync(EInkDisplay::FULL_REFRESH);
  display.waitRefreshComplete();
  delay(SPLASH_MS);
  drawScreen();
  display.displayBufferAsync(EInkDisplay::FULL_REFRESH);
  Serial.println("Ready.");

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

  requestRedraw(shell::update());
  handleInput();
  if (input.wasAnyPressed() || input.wasAnyReleased() || input.wasTouchActivity()) lastActivityMs = now;

  if (pendingRedraw != shell::Redraw::None && !display.refreshBusy()) {
    const shell::Redraw redraw = pendingRedraw;
    pendingRedraw = shell::Redraw::None;
    drawScreen();
    startRefresh(redraw);
  }

  // The press that woke the device must end before a hold can mean "sleep".
  const bool okDown = input.isPressed(InputManager::BTN_POWER);
  if (!okDown) releasedSinceBoot = true;

  if (releasedSinceBoot && okDown && input.getPowerButtonHeldTime() >= SLEEP_HOLD_MS) goToSleep();
  if (shell::busy()) lastActivityMs = now;
  if (now - lastActivityMs >= shell::idleSleepMs()) goToSleep();

  delay(POLL_MS);
}

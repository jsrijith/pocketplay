// BoardByte: shows the Sticky's buttons and touch input on screen, and keeps them
// on the SD card as a stand-in for a game in progress.
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

#include <cstdio>
#include <cstdlib>

#include "graphics.h"
#include "network.h"
#include "state.h"
#include "storage.h"
#include "text.h"
#include "wifi_screen.h"

namespace {

// InputManager doesn't debounce buttons on this board. Sampling further apart
// than a button bounces means one press can only produce one edge.
constexpr uint32_t POLL_MS = 15;
constexpr uint32_t SLEEP_HOLD_MS = 1000;
constexpr uint32_t CLEAR_HOLD_MS = 2000;

// Fast refreshes slowly leave ghosts behind; a full refresh clears them.
constexpr int FAST_REFRESHES_PER_FULL = 30;

constexpr const char* SETTINGS_FILE = "settings";
constexpr const char* GAME_FILE = "game";

constexpr int BOX_GAP = 12;
constexpr int BOX_MARGIN = 12;
constexpr int BOX_WIDTH = (SCREEN_WIDTH - 2 * BOX_MARGIN - 2 * BOX_GAP) / 3;
constexpr int BOX_HEIGHT = 130;
constexpr int BOX_TOP = 24;
constexpr int BOX_BORDER = 4;
constexpr int BOX_LABEL_HEIGHT = 56;

constexpr int SAMPLES_TOP = BOX_TOP + BOX_HEIGHT + 8;
constexpr int SAMPLES_MARGIN = 12;

constexpr int CARD_MESSAGE_TOP = SCREEN_HEIGHT - 254;
constexpr int STATUS_TOP = SCREEN_HEIGHT - 218;
constexpr int SLEEP_LINE = 3;
constexpr int WIFI_LINE = 4;
constexpr int LINE_TAP_WIDTH = 260;  // narrow, so the bottom corners still move the cross
constexpr int WIFI_LINE_MAX_WIDTH = SCREEN_WIDTH - 24;

constexpr int CROSS_ARM = 24;
constexpr int CROSS_THICKNESS = 4;

struct Sample {
  Font font;
  const char* text;
};

// Every character the fonts hold, then \x7F, which they don't, to show the missing
// character box. The groups are separated by spaces so the first line wraps at one.
constexpr Sample SAMPLES[] = {
    {Font::Size10, "!\"#$%&'()*+,-./ 0123456789 :;<=>?@ ABCDEFGHIJKLMNOPQRSTUVWXYZ [\\]^_` abcdefghijklmnopqrstuvwxyz {|}~\x7F"},
    {Font::Size14, "14 The quick brown fox jumps"},
    {Font::Size20, "20 HOW TO PLAY"},
    {Font::Size30, "30 YOUR TURN"},
    {Font::Size44, "44 WINNER"},
};

struct Button {
  const char* label;
  uint8_t index;  // InputManager's BTN_* value
};

// In on-screen order, left to right, matching GameState::buttonCounts. OK is read as
// BTN_POWER: setup() turns on the setting that makes it follow the button straight
// away, so it can count on press.
constexpr Button BUTTONS[BUTTON_COUNT] = {
    {"UP", InputManager::BTN_UP},
    {"OK", InputManager::BTN_POWER},
    {"DOWN", InputManager::BTN_DOWN},
};

InputManager input;

// The SDK's begin() takes its wiring from BoardConfig; these arguments are unused.
EInkDisplay display(BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED, BoardConfig::PIN_UNASSIGNED);

Settings settings;
GameState game;

bool settingsChanged = false;
bool gameChanged = false;
bool settingsSaveFailed = false;
bool gameSaveFailed = false;

bool ready = false;
bool screenDirty = false;
bool releasedSinceBoot = false;
int fastRefreshes = 0;
uint32_t lastActivityMs = 0;
uint32_t upPressedMs = 0;
bool upHoldUsed = true;  // only a press seen after boot can clear the save

const char* swipeName(Swipe swipe) {
  switch (swipe) {
    case Swipe::Left:
      return "LEFT";
    case Swipe::Right:
      return "RIGHT";
    case Swipe::Up:
      return "UP";
    case Swipe::Down:
      return "DOWN";
    case Swipe::None:
      break;
  }
  return "";
}

void drawButtonBox(uint8_t* frame, int index, int left) {
  const Button& button = BUTTONS[index];
  const bool held = input.isPressed(button.index);
  if (held) {
    fillRect(frame, left, BOX_TOP, BOX_WIDTH, BOX_HEIGHT, true);
  } else {
    drawRectOutline(frame, left, BOX_TOP, BOX_WIDTH, BOX_HEIGHT, BOX_BORDER);
  }

  const bool textBlack = !held;  // white text on a held button's black box
  drawTextInBox(frame, Font::Size20, left, BOX_TOP, BOX_WIDTH, BOX_LABEL_HEIGHT, button.label, textBlack);

  char count[6];
  snprintf(count, sizeof(count), "%u", game.buttonCounts[index]);
  drawTextInBox(frame, Font::Size30, left, BOX_TOP + BOX_LABEL_HEIGHT, BOX_WIDTH, BOX_HEIGHT - BOX_LABEL_HEIGHT, count, textBlack);
}

void drawSamples(uint8_t* frame) {
  int top = SAMPLES_TOP;
  for (const Sample& sample : SAMPLES) {
    top += drawWrapped(frame, sample.font, SAMPLES_MARGIN, top, SCREEN_WIDTH - 2 * SAMPLES_MARGIN, sample.text);
  }
}

void drawCross(uint8_t* frame, int x, int y) {
  fillRect(frame, x - CROSS_ARM, y - CROSS_THICKNESS / 2, CROSS_ARM * 2, CROSS_THICKNESS, true);
  fillRect(frame, x - CROSS_THICKNESS / 2, y - CROSS_ARM, CROSS_THICKNESS, CROSS_ARM * 2, true);
}

void drawStatusLine(uint8_t* frame, int line, const char* text) {
  const int height = lineHeight(Font::Size20);
  drawTextInBox(frame, Font::Size20, 0, STATUS_TOP + line * height, SCREEN_WIDTH, height, text);
}

void drawTouchStatus(uint8_t* frame) {
  if (!input.hasTouch()) {
    drawStatusLine(frame, 0, "NO TOUCH");
    return;
  }

  char text[32];
  if (game.hasTapped) {
    snprintf(text, sizeof(text), "X %d Y %d", game.tapX, game.tapY);
    drawStatusLine(frame, 0, text);
  }

  snprintf(text, sizeof(text), "TAPS %u", game.tapCount);
  drawStatusLine(frame, 1, text);

  if (game.swipe != Swipe::None) {
    snprintf(text, sizeof(text), "SWIPE %s %u", swipeName(game.swipe), game.swipeCount);
  } else {
    snprintf(text, sizeof(text), "SWIPE %u", game.swipeCount);
  }
  drawStatusLine(frame, 2, text);
}

void drawSleepLine(uint8_t* frame) {
  char text[16];
  snprintf(text, sizeof(text), "SLEEP %u MIN", settings.idleSleepMinutes);
  drawStatusLine(frame, SLEEP_LINE, text);
}

void drawWifiLine(uint8_t* frame) {
  char text[48];
  wifiScreen::statusLine(text, sizeof(text), WIFI_LINE_MAX_WIDTH);
  drawStatusLine(frame, WIFI_LINE, text);
}

void drawCardMessage(uint8_t* frame) {
  const char* message = nullptr;
  if (!storage::ready()) {
    message = "NO CARD";
  } else if (settingsSaveFailed || gameSaveFailed) {
    message = "SAVE FAILED";
  }
  if (message != nullptr) {
    drawTextInBox(frame, Font::Size14, 0, CARD_MESSAGE_TOP, SCREEN_WIDTH, lineHeight(Font::Size14), message);
  }
}

void drawInputScreen(uint8_t* frame) {
  int left = BOX_MARGIN;
  for (int i = 0; i < BUTTON_COUNT; i++) {
    drawButtonBox(frame, i, left);
    left += BOX_WIDTH + BOX_GAP;
  }

  drawSamples(frame);
  drawCardMessage(frame);
  drawTouchStatus(frame);
  drawSleepLine(frame);
  drawWifiLine(frame);
  if (game.hasTapped) drawCross(frame, game.tapX, game.tapY);
}

void drawScreen() {
  const uint32_t startUs = micros();

  display.clearScreen(0xFF);
  uint8_t* frame = display.getFrameBuffer();

  if (wifiScreen::isOpen()) {
    wifiScreen::draw(frame);
  } else {
    drawInputScreen(frame);
  }

  Serial.printf("Screen drawn in %lu us\n", static_cast<unsigned long>(micros() - startUs));
}

// Writing doesn't happen without a card, so a missing card isn't reported as a failure.
bool saveFile(const char* name, uint8_t version, const uint8_t* data, size_t size) {
  if (!storage::ready()) return true;

  const uint32_t startUs = micros();
  const bool saved = storage::save(name, version, data, size);
  Serial.printf("Saved %s in %lu us\n", name, static_cast<unsigned long>(micros() - startUs));
  return saved;
}

void saveSettings() {
  uint8_t data[SETTINGS_BYTES];
  const size_t size = packSettings(settings, data);
  settingsSaveFailed = !saveFile(SETTINGS_FILE, SETTINGS_VERSION, data, size);
}

void saveGame() {
  uint8_t data[GAME_BYTES];
  const size_t size = packGame(game, data);
  gameSaveFailed = !saveFile(GAME_FILE, GAME_VERSION, data, size);
}

// A file that's missing or can't be trusted just leaves the defaults in place.
void loadSaved() {
  uint8_t data[storage::MAX_DATA_BYTES];
  uint8_t version = 0;
  size_t size = 0;

  if (storage::load(SETTINGS_FILE, version, data, sizeof(data), size) && !unpackSettings(version, data, size, settings)) {
    Serial.println("Ignoring saved settings");
  }
  if (storage::load(GAME_FILE, version, data, sizeof(data), size) && !unpackGame(version, data, size, game)) {
    Serial.println("Ignoring saved game");
  }
}

// Stands in for a game ending: its save mustn't outlive it. If erasing fails, the
// zeroed state gets saved over it on the next retry instead.
void clearGame() {
  game = GameState{};
  gameSaveFailed = storage::ready() && !storage::erase(GAME_FILE);
  Serial.println("Game cleared");
}

// Returns true if anything on screen needs to change.
bool handleButtons(uint32_t now) {
  for (int i = 0; i < BUTTON_COUNT; i++) {
    if (!input.wasPressed(BUTTONS[i].index)) continue;
    game.buttonCounts[i] = (game.buttonCounts[i] + 1) % COUNT_LIMIT;
    gameChanged = true;
    Serial.printf("%s pressed, count %u\n", BUTTONS[i].label, game.buttonCounts[i]);
  }

  if (input.wasPressed(InputManager::BTN_UP)) {
    upPressedMs = now;
    upHoldUsed = false;
  }
  if (!upHoldUsed && input.isPressed(InputManager::BTN_UP) && now - upPressedMs >= CLEAR_HOLD_MS) {
    upHoldUsed = true;
    gameChanged = false;
    clearGame();
    return true;
  }

  return input.wasAnyPressed() || input.wasAnyReleased();
}

// Touch positions arrive as 0..1 in the panel's landscape frame.
int touchToScreenX(float panelY) { return static_cast<int>((1.0f - panelY) * (SCREEN_WIDTH - 1)); }
int touchToScreenY(float panelX) { return static_cast<int>(panelX * (SCREEN_HEIGHT - 1)); }

bool isOnStatusLine(int line, int x, int y) {
  const int height = lineHeight(Font::Size20);
  const int top = STATUS_TOP + line * height;
  return y >= top && y < top + height && abs(x - SCREEN_WIDTH / 2) <= LINE_TAP_WIDTH / 2;
}

void cycleIdleSleep() {
  settings.idleSleepMinutes = settings.idleSleepMinutes == 1 ? 5 : settings.idleSleepMinutes == 5 ? 10 : 1;
  settingsChanged = true;
  Serial.printf("Idle sleep is now %u minutes\n", settings.idleSleepMinutes);
}

// Returns true if anything on screen needs to change. A swipe and a tap can't
// both fire for one touch: a tap has to move less than a swipe's minimum distance.
bool handleTouch() {
  float startX, startY, endX, endY;
  if (input.wasSwipe(startX, startY, endX, endY)) {
    const int dx = touchToScreenX(endY) - touchToScreenX(startY);
    const int dy = touchToScreenY(endX) - touchToScreenY(startX);
    if (abs(dx) >= abs(dy)) {
      game.swipe = dx < 0 ? Swipe::Left : Swipe::Right;
    } else {
      game.swipe = dy < 0 ? Swipe::Up : Swipe::Down;
    }
    game.swipeCount = (game.swipeCount + 1) % COUNT_LIMIT;
    gameChanged = true;
    Serial.printf("SWIPE %s\n", swipeName(game.swipe));
    return true;
  }

  float x, y;
  if (input.wasTouchTap(x, y)) {
    const int screenX = touchToScreenX(y);
    const int screenY = touchToScreenY(x);
    if (isOnStatusLine(SLEEP_LINE, screenX, screenY)) {
      cycleIdleSleep();
      return true;
    }
    if (isOnStatusLine(WIFI_LINE, screenX, screenY)) {
      wifiScreen::open();
      return true;
    }
    game.tapX = static_cast<int16_t>(screenX);
    game.tapY = static_cast<int16_t>(screenY);
    game.hasTapped = true;
    game.tapCount = (game.tapCount + 1) % COUNT_LIMIT;
    gameChanged = true;
    Serial.printf("TAP %d %d\n", screenX, screenY);
    return true;
  }

  return false;
}

// The stand-in game only changes on the input screen, so here buttons and touch go
// to the Wi-Fi screens instead. Returns true if anything on screen needs to change.
bool handleWifiScreenInput() {
  upHoldUsed = true;  // a hold of UP that began here doesn't clear the save

  bool changed = false;
  if (input.wasPressed(InputManager::BTN_UP)) changed = wifiScreen::onUpPressed() || changed;
  if (input.wasPressed(InputManager::BTN_DOWN)) changed = wifiScreen::onDownPressed() || changed;

  float x, y;
  if (input.wasTouchTap(x, y)) changed = wifiScreen::onTap(touchToScreenX(y), touchToScreenY(x)) || changed;
  return changed;
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
  drawTextInBox(display.getFrameBuffer(), Font::Size64, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, "ASLEEP");

  // Async even though it waits: after an async refresh the panel's stored
  // previous frame is out of date, and only this path sends the correct one.
  display.displayBufferAsync(EInkDisplay::FAST_REFRESH);
  display.waitRefreshComplete();
}

[[noreturn]] void goToSleep() {
  // Otherwise the modem's power domain could stay on through deep sleep.
  network::turnOff();

  // Every change is saved as it happens, so this only retries one that failed.
  if (settingsSaveFailed) saveSettings();
  if (gameSaveFailed) saveGame();

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

  if (storage::begin()) loadSaved();
  wifiScreen::begin();

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

  if (wifiScreen::update()) screenDirty = true;

  // Input can open or close the Wi-Fi screen, so that also needs a redraw.
  if (wifiScreen::isOpen()) {
    if (handleWifiScreenInput()) screenDirty = true;
  } else {
    const bool buttonsChanged = handleButtons(now);
    const bool touchChanged = handleTouch();
    if (buttonsChanged || touchChanged) screenDirty = true;
  }
  if (input.wasAnyPressed() || input.wasAnyReleased() || input.wasTouchActivity()) lastActivityMs = now;

  // Before the redraw, so the screen can report a save that failed.
  if (settingsChanged) {
    settingsChanged = false;
    saveSettings();
  }
  if (gameChanged) {
    gameChanged = false;
    saveGame();
  }

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
  if (now - lastActivityMs >= settings.idleSleepMinutes * 60UL * 1000UL) goToSleep();

  delay(POLL_MS);
}

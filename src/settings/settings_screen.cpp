#include "settings_screen.h"

#include <Arduino.h>

#include <cstdio>

#include "core/graphics.h"
#include "core/storage.h"
#include "state.h"
#include "ui/ui.h"
#include "wifi_screen.h"

namespace settingsScreen {

namespace {

constexpr const char* SETTINGS_FILE = "settings";

constexpr int ROW_WIDTH = SCREEN_WIDTH - 2 * ui::MARGIN;
constexpr int ROW_HEIGHT = 84;
constexpr int WIFI_ROW_TOP = ui::CONTENT_TOP;
constexpr int SLEEP_ROW_TOP = WIFI_ROW_TOP + ROW_HEIGHT + ui::GAP;
constexpr int ROW_TEXT_WIDTH = ROW_WIDTH - 2 * ui::GAP;

Settings settings;
bool failed = false;

void save() {
  if (!storage::ready()) {
    failed = false;
    return;
  }
  uint8_t data[SETTINGS_BYTES];
  const size_t size = packSettings(settings, data);
  failed = !storage::save(SETTINGS_FILE, SETTINGS_VERSION, data, size);
}

void cycleIdleSleep() {
  settings.idleSleepMinutes = settings.idleSleepMinutes == 1 ? 5 : settings.idleSleepMinutes == 5 ? 10 : 1;
  Serial.printf("Idle sleep is now %u minutes\n", settings.idleSleepMinutes);
  save();
}

}  // namespace

// A file that's missing or can't be trusted just leaves the defaults in place.
void begin() {
  if (!storage::ready()) return;

  uint8_t data[SETTINGS_BYTES];
  uint8_t version = 0;
  size_t size = 0;
  if (storage::load(SETTINGS_FILE, version, data, sizeof(data), size) && !unpackSettings(version, data, size, settings)) {
    Serial.println("Ignoring saved settings");
  }
}

uint8_t idleSleepMinutes() { return settings.idleSleepMinutes; }

Tap onTap(int x, int y) {
  if (ui::isOnBack(x, y)) return Tap::Back;
  if (ui::isInBox(x, y, ui::MARGIN, WIFI_ROW_TOP, ROW_WIDTH, ROW_HEIGHT)) return Tap::OpenWifi;
  if (ui::isInBox(x, y, ui::MARGIN, SLEEP_ROW_TOP, ROW_WIDTH, ROW_HEIGHT)) {
    cycleIdleSleep();
    return Tap::Changed;
  }
  return Tap::Nothing;
}

void draw(uint8_t* frame) {
  ui::drawHeader(frame, "SETTINGS");

  char text[48];
  wifiScreen::statusLine(text, sizeof(text), ROW_TEXT_WIDTH);
  ui::drawButton(frame, ui::MARGIN, WIFI_ROW_TOP, ROW_WIDTH, ROW_HEIGHT, text);

  snprintf(text, sizeof(text), "SLEEP %u MIN", settings.idleSleepMinutes);
  ui::drawButton(frame, ui::MARGIN, SLEEP_ROW_TOP, ROW_WIDTH, ROW_HEIGHT, text);
}

bool saveFailed() { return failed; }

void retrySave() { save(); }

}  // namespace settingsScreen

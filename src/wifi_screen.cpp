#include "wifi_screen.h"

#include <Arduino.h>
#include <esp_mac.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "graphics.h"
#include "keyboard.h"
#include "network.h"
#include "state.h"
#include "storage.h"
#include "text.h"
#include "ui.h"

namespace wifiScreen {

namespace {

constexpr const char* NETWORKS_FILE = "networks";

// A remembered network gets a shorter limit, so a network that's out of range quickly
// leads to the list. Typed passwords get longer, as the router may just be slow.
constexpr uint32_t SAVED_JOIN_TIMEOUT_MS = 7000;
constexpr uint32_t TYPED_JOIN_TIMEOUT_MS = 15000;

using ui::drawButton;
using ui::isInBox;

constexpr int MARGIN = ui::MARGIN;
constexpr int BUTTON_GAP = ui::GAP;

constexpr int HEADER_TOP = ui::HEADER_TOP;
constexpr int HEADER_BUTTON_WIDTH = ui::HEADER_BUTTON_WIDTH;
constexpr int HEADER_BUTTON_HEIGHT = ui::HEADER_BUTTON_HEIGHT;
constexpr int ROWS_TOP = ui::CONTENT_TOP;
constexpr int ROW_HEIGHT = 72;
constexpr int ROWS_PER_PAGE = 8;
constexpr int PAGE_LABEL_TOP = ROWS_TOP + ROWS_PER_PAGE * ROW_HEIGHT + 8;

constexpr int BAR_WIDTH = 8;
constexpr int BAR_GAP = 3;
constexpr int BAR_COUNT = 4;
constexpr int BAR_STEP_HEIGHT = 9;
constexpr int BARS_WIDTH = BAR_COUNT * BAR_WIDTH + (BAR_COUNT - 1) * BAR_GAP;
constexpr int MARK_WIDTH = 70;

constexpr int STATUS_TITLE_TOP = 40;
constexpr int STATUS_LINES_TOP = 170;
constexpr int STATUS_BUTTON_WIDTH = (SCREEN_WIDTH - 2 * MARGIN - BUTTON_GAP) / 2;
constexpr int STATUS_BUTTON_HEIGHT = 84;
constexpr int STATUS_BUTTONS_TOP = SCREEN_HEIGHT - MARGIN - 2 * STATUS_BUTTON_HEIGHT - BUTTON_GAP;

enum class View { List, Status, Keyboard };

enum class Action { Back, TurnOff, Change, Forget, TryAgain, Password };

struct StatusButton {
  Action action;
  const char* label;
};

constexpr int MAX_STATUS_BUTTONS = 4;

bool screenOpen = false;
View view = View::Status;
View keyboardReturn = View::List;
int page = 0;
const char* listMessage = "";  // why the list opened, after a remembered network failed

SavedNetworks saved;
const char* rememberProblem = "";

// The network being joined. The password stays until the next attempt, for TRY AGAIN
// and for remembering it once the join works. It never goes to the log.
char joinSsid[MAX_SSID_BYTES + 1] = "";
char joinPassword[MAX_PASSWORD_BYTES + 1] = "";
bool joinFromOpen = false;  // opened from the input screen, so a failure leads to the list

time_t shownMinute = 0;

void scrambleKey(uint8_t key[NETWORK_KEY_BYTES]) { esp_efuse_mac_get_default(key); }

void loadNetworks() {
  uint8_t data[storage::MAX_DATA_BYTES];
  uint8_t version = 0;
  size_t size = 0;
  uint8_t key[NETWORK_KEY_BYTES];
  scrambleKey(key);

  if (storage::load(NETWORKS_FILE, version, data, sizeof(data), size) && !unpackNetworks(version, data, size, key, saved)) {
    Serial.println("Ignoring saved networks");
  }
}

void saveNetworks() {
  rememberProblem = "";
  if (!storage::ready()) {
    rememberProblem = "NOT REMEMBERED: NO CARD";
    return;
  }

  uint8_t data[NETWORKS_MAX_BYTES];
  uint8_t key[NETWORK_KEY_BYTES];
  scrambleKey(key);
  const size_t size = packNetworks(saved, key, data);
  if (!storage::save(NETWORKS_FILE, NETWORKS_VERSION, data, size)) {
    rememberProblem = "NOT REMEMBERED: SAVE FAILED";
  }
}

// Remembered networks first, then by strength.
void sortResults() {
  int count = 0;
  network::ScanEntry* entries = network::results(count);
  std::sort(entries, entries + count, [](const network::ScanEntry& a, const network::ScanEntry& b) {
    const bool aSaved = findNetwork(saved, a.ssid) != nullptr;
    const bool bSaved = findNetwork(saved, b.ssid) != nullptr;
    if (aSaved != bSaved) return aSaved;
    return a.rssi > b.rssi;
  });
}

void startScan() {
  page = 0;
  view = View::List;
  network::startScan();
}

void startJoin(uint32_t timeoutMs) {
  rememberProblem = "";
  view = View::Status;
  network::join(joinSsid, joinPassword, timeoutMs);
}

void setJoinTarget(const char* ssid, const char* password) {
  snprintf(joinSsid, sizeof(joinSsid), "%s", ssid);
  snprintf(joinPassword, sizeof(joinPassword), "%s", password);
}

void openKeyboard(View returnTo) {
  keyboard::begin(joinSsid);
  keyboardReturn = returnTo;
  view = View::Keyboard;
}

void pickNetwork(int index) {
  int count = 0;
  const network::ScanEntry* entries = network::results(count);
  if (index < 0 || index >= count) return;
  const network::ScanEntry& entry = entries[index];

  listMessage = "";
  joinFromOpen = false;
  if (const SavedNetwork* known = findNetwork(saved, entry.ssid)) {
    setJoinTarget(known->ssid, known->password);
    startJoin(SAVED_JOIN_TIMEOUT_MS);
  } else if (!entry.locked) {
    setJoinTarget(entry.ssid, "");
    startJoin(TYPED_JOIN_TIMEOUT_MS);
  } else {
    setJoinTarget(entry.ssid, "");
    openKeyboard(View::List);
  }
}

void forgetCurrent() {
  if (forgetNetwork(saved, network::ssid())) saveNetworks();

  // The screen closes next, so the log is the only place left to report this.
  if (rememberProblem[0] != '\0') Serial.println("Forgotten network not saved to the card");
  rememberProblem = "";
}

int statusButtons(StatusButton out[MAX_STATUS_BUTTONS]) {
  int count = 0;
  if (network::state() == network::State::Failed) {
    out[count++] = {Action::TryAgain, "TRY AGAIN"};
    if (joinPassword[0] != '\0') out[count++] = {Action::Password, "PASSWORD"};
    if (findNetwork(saved, joinSsid) != nullptr) out[count++] = {Action::Forget, "FORGET"};
    out[count++] = {Action::Back, "BACK"};
    return count;
  }

  out[count++] = {Action::Back, "BACK"};
  out[count++] = {Action::TurnOff, "TURN OFF"};
  out[count++] = {Action::Change, "CHANGE"};
  if (findNetwork(saved, network::ssid()) != nullptr) out[count++] = {Action::Forget, "FORGET"};
  return count;
}

void statusButtonBox(int index, int& left, int& top) {
  left = MARGIN + (index % 2) * (STATUS_BUTTON_WIDTH + BUTTON_GAP);
  top = STATUS_BUTTONS_TOP + (index / 2) * (STATUS_BUTTON_HEIGHT + BUTTON_GAP);
}

void runAction(Action action) {
  switch (action) {
    case Action::Back:
      // A failed join has already turned the radio off, so this only clears the failure.
      if (network::state() == network::State::Failed) network::turnOff();
      screenOpen = false;
      break;
    case Action::TurnOff:
      network::turnOff();
      screenOpen = false;
      break;
    case Action::Change:
      listMessage = "";
      startScan();
      break;
    case Action::Forget:
      forgetCurrent();
      network::turnOff();
      screenOpen = false;
      break;
    case Action::TryAgain:
      joinFromOpen = false;
      startJoin(TYPED_JOIN_TIMEOUT_MS);
      break;
    case Action::Password:
      openKeyboard(View::Status);
      break;
  }
}

bool tapStatus(int x, int y) {
  StatusButton buttons[MAX_STATUS_BUTTONS];
  const int count = statusButtons(buttons);
  for (int i = 0; i < count; i++) {
    int left, top;
    statusButtonBox(i, left, top);
    if (isInBox(x, y, left, top, STATUS_BUTTON_WIDTH, STATUS_BUTTON_HEIGHT)) {
      runAction(buttons[i].action);
      return true;
    }
  }
  return false;
}

bool tapList(int x, int y) {
  if (ui::isOnBack(x, y)) {
    listMessage = "";
    network::turnOff();
    screenOpen = false;
    return true;
  }

  // Taps during a scan would land on rows that aren't there yet.
  if (network::state() != network::State::ScanDone) return false;

  if (isInBox(x, y, SCREEN_WIDTH - MARGIN - HEADER_BUTTON_WIDTH, HEADER_TOP, HEADER_BUTTON_WIDTH, HEADER_BUTTON_HEIGHT)) {
    listMessage = "";
    startScan();
    return true;
  }
  if (y >= ROWS_TOP && y < ROWS_TOP + ROWS_PER_PAGE * ROW_HEIGHT) {
    pickNetwork(page * ROWS_PER_PAGE + (y - ROWS_TOP) / ROW_HEIGHT);
    return true;
  }
  return false;
}

bool tapKeyboard(int x, int y) {
  switch (keyboard::onTap(x, y)) {
    case keyboard::Event::None:
      return false;
    case keyboard::Event::Changed:
      return true;
    case keyboard::Event::Back:
      view = keyboardReturn;
      return true;
    case keyboard::Event::Join:
      snprintf(joinPassword, sizeof(joinPassword), "%s", keyboard::text());
      joinFromOpen = false;
      startJoin(TYPED_JOIN_TIMEOUT_MS);
      return true;
  }
  return false;
}

// Filled bars for how many of four signal levels this reaches. The thresholds are
// the usual rough guide: -55 dBm is excellent and -85 dBm barely works.
void drawSignalBars(uint8_t* frame, int right, int centerY, int rssi) {
  const int level = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : 1;
  const int bottom = centerY + (BAR_COUNT * BAR_STEP_HEIGHT) / 2;
  for (int i = 0; i < BAR_COUNT; i++) {
    const int height = (i + 1) * BAR_STEP_HEIGHT;
    const int left = right - BARS_WIDTH + i * (BAR_WIDTH + BAR_GAP);
    if (i < level) {
      fillRect(frame, left, bottom - height, BAR_WIDTH, height, true);
    } else {
      drawRectOutline(frame, left, bottom - height, BAR_WIDTH, height, 1);
    }
  }
}

void drawRow(uint8_t* frame, const network::ScanEntry& entry, int top) {
  const int centerY = top + ROW_HEIGHT / 2;
  const int right = SCREEN_WIDTH - MARGIN;
  drawSignalBars(frame, right, centerY, entry.rssi);

  const char* mark = findNetwork(saved, entry.ssid) != nullptr ? "SAVED" : !entry.locked ? "OPEN" : nullptr;
  const int markLeft = right - BARS_WIDTH - 8 - MARK_WIDTH;
  if (mark != nullptr) drawTextInBox(frame, Font::Size14, markLeft, top, MARK_WIDTH, ROW_HEIGHT, mark);

  char name[MAX_SSID_BYTES + 3];
  fitText(Font::Size20, entry.ssid, markLeft - MARGIN - 8, name, sizeof(name));
  const int nameBaseline = centerY + capHeight(Font::Size20) / 2;
  drawText(frame, Font::Size20, MARGIN, nameBaseline, name);

  fillRect(frame, MARGIN, top + ROW_HEIGHT - 1, SCREEN_WIDTH - 2 * MARGIN, 1, true);
}

void drawList(uint8_t* frame) {
  ui::drawHeader(frame, "WI-FI", true);

  if (listMessage[0] != '\0') ui::drawWarning(frame, listMessage);

  if (network::state() != network::State::ScanDone) {
    drawTextInBox(frame, Font::Size30, 0, ROWS_TOP, SCREEN_WIDTH, ROW_HEIGHT * 2, "SCANNING");
    return;
  }

  drawButton(frame, SCREEN_WIDTH - MARGIN - HEADER_BUTTON_WIDTH, HEADER_TOP, HEADER_BUTTON_WIDTH, HEADER_BUTTON_HEIGHT, "RESCAN");

  int count = 0;
  const network::ScanEntry* entries = network::results(count);
  if (count == 0) {
    drawTextInBox(frame, Font::Size20, 0, ROWS_TOP, SCREEN_WIDTH, ROW_HEIGHT, "NO NETWORKS FOUND");
    drawTextInBox(frame, Font::Size14, 0, ROWS_TOP + ROW_HEIGHT, SCREEN_WIDTH, lineHeight(Font::Size14), "THIS CHIP ONLY SEES 2.4 GHZ");
    return;
  }

  for (int i = 0; i < ROWS_PER_PAGE; i++) {
    const int index = page * ROWS_PER_PAGE + i;
    if (index >= count) break;
    drawRow(frame, entries[index], ROWS_TOP + i * ROW_HEIGHT);
  }

  const int pages = (count + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
  if (pages > 1) {
    char label[48];
    snprintf(label, sizeof(label), "PAGE %d OF %d: UP AND DOWN", page + 1, pages);
    drawTextInBox(frame, Font::Size14, 0, PAGE_LABEL_TOP, SCREEN_WIDTH, lineHeight(Font::Size14), label);
  }
}

void drawStatusLine(uint8_t* frame, int line, const char* text) {
  const int height = lineHeight(Font::Size20);
  drawTextInBox(frame, Font::Size20, 0, STATUS_LINES_TOP + line * height, SCREEN_WIDTH, height, text);
}

void drawConnected(uint8_t* frame) {
  char text[48];
  drawStatusLine(frame, 0, "CONNECTED");

  char ip[16];
  network::ipText(ip, sizeof(ip));
  snprintf(text, sizeof(text), "IP %s", ip);
  drawStatusLine(frame, 1, text);

  snprintf(text, sizeof(text), "SIGNAL %d DBM", network::rssi());
  drawStatusLine(frame, 2, text);

  switch (network::internet()) {
    case network::Internet::Waiting:
      drawStatusLine(frame, 3, "CHECKING INTERNET");
      break;
    case network::Internet::NotReached:
      drawStatusLine(frame, 3, "NO INTERNET");
      break;
    case network::Internet::Reached: {
      const time_t now = time(nullptr);
      tm utc;
      gmtime_r(&now, &utc);
      snprintf(text, sizeof(text), "%04d-%02d-%02d", utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday);
      drawStatusLine(frame, 3, text);
      snprintf(text, sizeof(text), "TIME %02d:%02d UTC", utc.tm_hour, utc.tm_min);
      drawStatusLine(frame, 4, text);
      break;
    }
  }
}

void drawStatus(uint8_t* frame) {
  char title[MAX_SSID_BYTES + 3];
  fitText(Font::Size30, network::ssid(), SCREEN_WIDTH - 2 * MARGIN, title, sizeof(title));
  drawTextInBox(frame, Font::Size30, 0, STATUS_TITLE_TOP, SCREEN_WIDTH, lineHeight(Font::Size30), title);

  switch (network::state()) {
    case network::State::Joining:
      drawStatusLine(frame, 0, "JOINING");
      break;
    case network::State::Connected:
      drawConnected(frame);
      break;
    case network::State::Lost:
      drawStatusLine(frame, 0, "LOST");
      drawStatusLine(frame, 1, "TRYING TO RECONNECT");
      break;
    case network::State::Failed:
      drawStatusLine(frame, 0, "COULD NOT JOIN");
      drawStatusLine(frame, 1, network::failureText());
      break;
    case network::State::Off:
    case network::State::Scanning:
    case network::State::ScanDone:
      break;
  }

  if (rememberProblem[0] != '\0') {
    const int top = STATUS_BUTTONS_TOP - lineHeight(Font::Size14) - BUTTON_GAP;
    drawTextInBox(frame, Font::Size14, 0, top, SCREEN_WIDTH, lineHeight(Font::Size14), rememberProblem);
  }

  StatusButton buttons[MAX_STATUS_BUTTONS];
  const int count = statusButtons(buttons);
  for (int i = 0; i < count; i++) {
    int left, top;
    statusButtonBox(i, left, top);
    drawButton(frame, left, top, STATUS_BUTTON_WIDTH, STATUS_BUTTON_HEIGHT, buttons[i].label);
  }
}

// The time is only worth a redraw while it's on screen and has changed.
bool minuteChanged() {
  if (!screenOpen || view != View::Status) return false;
  if (network::state() != network::State::Connected || network::internet() != network::Internet::Reached) return false;

  const time_t minute = time(nullptr) / 60;
  if (minute == shownMinute) return false;
  shownMinute = minute;
  return true;
}

}  // namespace

void begin() {
  if (storage::ready()) loadNetworks();
}

bool update() {
  const network::State before = network::state();
  bool changed = network::update();
  const network::State now = network::state();

  if (before == network::State::Joining && now == network::State::Connected) {
    // Only a network that has worked is remembered, so a mistyped password never is.
    if (rememberNetwork(saved, joinSsid, joinPassword)) saveNetworks();
  }

  if (before == network::State::Joining && now == network::State::Failed && joinFromOpen) {
    joinFromOpen = false;
    if (screenOpen) {
      listMessage = network::failureText();
      startScan();
    }
    changed = true;
  }

  if (before == network::State::Scanning && now == network::State::ScanDone) sortResults();

  return minuteChanged() || changed;
}

void open() {
  screenOpen = true;
  listMessage = "";

  switch (network::state()) {
    case network::State::Joining:
    case network::State::Connected:
    case network::State::Lost:
      view = View::Status;
      return;
    case network::State::Scanning:
      view = View::List;
      return;
    case network::State::Off:
    case network::State::ScanDone:
    case network::State::Failed:
      break;
  }

  if (saved.count > 0) {
    setJoinTarget(saved.items[0].ssid, saved.items[0].password);
    joinFromOpen = true;
    startJoin(SAVED_JOIN_TIMEOUT_MS);
  } else {
    startScan();
  }
}

bool isOpen() { return screenOpen; }

bool onTap(int x, int y) {
  switch (view) {
    case View::Status:
      return tapStatus(x, y);
    case View::List:
      return tapList(x, y);
    case View::Keyboard:
      return tapKeyboard(x, y);
  }
  return false;
}

bool onUpPressed() {
  if (view != View::List || page == 0) return false;
  page--;
  return true;
}

bool onDownPressed() {
  if (view != View::List || network::state() != network::State::ScanDone) return false;
  int count = 0;
  network::results(count);
  if ((page + 1) * ROWS_PER_PAGE >= count) return false;
  page++;
  return true;
}

void draw(uint8_t* frame) {
  switch (view) {
    case View::Status:
      drawStatus(frame);
      break;
    case View::List:
      drawList(frame);
      break;
    case View::Keyboard:
      keyboard::draw(frame);
      break;
  }
}

void statusLine(char* out, size_t size, int maxWidth) {
  char text[48];
  switch (network::state()) {
    case network::State::Joining:
      snprintf(text, sizeof(text), "WIFI JOINING");
      break;
    case network::State::Connected:
      snprintf(text, sizeof(text), "WIFI %s", network::ssid());
      break;
    case network::State::Lost:
      snprintf(text, sizeof(text), "WIFI LOST");
      break;
    case network::State::Failed:
      snprintf(text, sizeof(text), "WIFI FAILED");
      break;
    case network::State::Off:
    case network::State::Scanning:
    case network::State::ScanDone:
      snprintf(text, sizeof(text), "WIFI OFF");
      break;
  }
  fitText(Font::Size20, text, maxWidth, out, size);
}

}  // namespace wifiScreen

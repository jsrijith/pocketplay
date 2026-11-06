#include "network.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <lwip/priv/tcpip_priv.h>

#include <cstdio>
#include <cstring>

#include "clock.h"

namespace network {

namespace {

// How long a time server gets before the screen says there's no internet. The
// request keeps retrying after this, so a late answer still counts.
constexpr uint32_t INTERNET_TIMEOUT_MS = 15000;

// A brand new join can't have failed this soon, so a failure reported earlier
// than this is about something else.
constexpr uint32_t MIN_JOIN_MS = 1000;

constexpr const char* HOSTNAME = "PocketPlay";

State currentState = State::Off;
Internet currentInternet = Internet::Waiting;
bool radioOn = false;
bool timeStarted = false;

ScanEntry scanResults[MAX_SCAN_RESULTS];
int scanCount = 0;

char currentSsid[MAX_SSID_BYTES + 1] = "";
const char* failure = "";

uint32_t joinStartMs = 0;
uint32_t joinTimeoutMs = 0;
uint32_t connectedMs = 0;
wl_status_t lastJoinStatus = WL_IDLE_STATUS;

void stopTimeCheck() {
  if (!timeStarted) return;
  timeStarted = false;

  // configTzTime() holds this lock around the same calls.
  LOCK_TCPIP_CORE();
  if (esp_sntp_enabled()) esp_sntp_stop();
  UNLOCK_TCPIP_CORE();
}

void startTimeCheck() {
  currentInternet = Internet::Waiting;
  connectedMs = millis();
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  // Not configTime(), which replaces the time zone with a plain offset.
  configTzTime(wallClock::posixTz(), "pool.ntp.org", "time.google.com");
  timeStarted = true;
}

void stopRadio() {
  stopTimeCheck();
  if (!radioOn) return;
  radioOn = false;

  WiFi.scanDelete();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// Always starts from off, so no status from an earlier attempt can be mistaken
// for this one. With persistent(false) the Wi-Fi driver neither stores a password
// in flash nor connects by itself to one another firmware stored there.
bool startRadio() {
  stopRadio();
  WiFi.persistent(false);
  WiFi.setHostname(HOSTNAME);
  radioOn = WiFi.mode(WIFI_STA);
  return radioOn;
}

void failJoin(const char* reason) {
  Serial.printf("Could not join %s: %s\n", currentSsid, reason);
  failure = reason;
  stopRadio();
  currentState = State::Failed;
}

bool canJoin(wifi_auth_mode_t mode) {
  switch (mode) {
    case WIFI_AUTH_ENTERPRISE:
    case WIFI_AUTH_WPA3_ENT_192:
    case WIFI_AUTH_WPA3_ENTERPRISE:
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
    case WIFI_AUTH_WPA_ENTERPRISE:
      return false;  // these need a username and certificates
    default:
      return true;
  }
}

// One network name can have several access points, so this keeps the strongest. When
// the list is full, a stronger network replaces the weakest.
void addScanResult(const char* ssid, int rssi, bool locked) {
  const int8_t signal = static_cast<int8_t>(rssi);

  for (int i = 0; i < scanCount; i++) {
    if (strcmp(scanResults[i].ssid, ssid) != 0) continue;
    if (signal > scanResults[i].rssi) {
      scanResults[i].rssi = signal;
      scanResults[i].locked = locked;
    }
    return;
  }

  int slot = scanCount;
  if (scanCount == MAX_SCAN_RESULTS) {
    slot = 0;
    for (int i = 1; i < scanCount; i++) {
      if (scanResults[i].rssi < scanResults[slot].rssi) slot = i;
    }
    if (signal <= scanResults[slot].rssi) return;
  } else {
    scanCount++;
  }

  snprintf(scanResults[slot].ssid, sizeof(scanResults[slot].ssid), "%s", ssid);
  scanResults[slot].rssi = signal;
  scanResults[slot].locked = locked;
}

bool updateScan() {
  const int16_t found = WiFi.scanComplete();
  if (found == WIFI_SCAN_RUNNING) return false;

  for (int i = 0; i < found; i++) {
    const String name = WiFi.SSID(i);
    const wifi_auth_mode_t mode = WiFi.encryptionType(i);
    if (name.isEmpty() || !canJoin(mode)) continue;  // an empty name is a hidden network
    addScanResult(name.c_str(), WiFi.RSSI(i), mode != WIFI_AUTH_OPEN);
  }

  Serial.printf("Scan found %d networks, listing %d\n", found, scanCount);
  stopRadio();
  currentState = State::ScanDone;
  return true;
}

// A wrong password usually doesn't report a failure of its own: the driver keeps
// retrying, and the time limit ends it. The last status seen says what to suspect.
bool updateJoin(uint32_t now) {
  const wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    Serial.printf("Joined %s in %lu ms\n", currentSsid, static_cast<unsigned long>(now - joinStartMs));
    currentState = State::Connected;
    startTimeCheck();
    return true;
  }

  if (status != WL_IDLE_STATUS && status != WL_DISCONNECTED) lastJoinStatus = status;

  const uint32_t elapsed = now - joinStartMs;
  if (elapsed >= MIN_JOIN_MS && status == WL_CONNECT_FAILED) {
    failJoin("CHECK THE PASSWORD");
    return true;
  }
  if (elapsed < joinTimeoutMs) return false;

  if (lastJoinStatus == WL_NO_SSID_AVAIL) {
    failJoin("NETWORK NOT FOUND");
  } else if (lastJoinStatus == WL_CONNECTION_LOST || lastJoinStatus == WL_CONNECT_FAILED) {
    failJoin("CHECK THE PASSWORD");
  } else {
    failJoin("NO ANSWER");
  }
  return true;
}

// sntp_get_sync_status() clears itself once read, so a success has to be kept.
bool updateInternet(uint32_t now) {
  if (currentInternet == Internet::Reached) return false;

  if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
    Serial.printf("Time server answered after %lu ms\n", static_cast<unsigned long>(now - connectedMs));
    currentInternet = Internet::Reached;
    wallClock::saveToChip();
    return true;
  }
  if (currentInternet == Internet::Waiting && now - connectedMs >= INTERNET_TIMEOUT_MS) {
    Serial.println("No answer from a time server");
    currentInternet = Internet::NotReached;
    return true;
  }
  return false;
}

}  // namespace

void startScan() {
  scanCount = 0;
  if (!startRadio() || WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
    Serial.println("Could not start a scan");
    stopRadio();
    currentState = State::ScanDone;
    return;
  }
  currentState = State::Scanning;
}

void join(const char* ssid, const char* password, uint32_t timeoutMs) {
  snprintf(currentSsid, sizeof(currentSsid), "%s", ssid);
  joinStartMs = millis();
  joinTimeoutMs = timeoutMs;
  lastJoinStatus = WL_IDLE_STATUS;

  if (!startRadio()) {
    failJoin("RADIO DID NOT START");
    return;
  }

  // Looks at every channel and takes the strongest access point with this name,
  // rather than the first one it happens to find.
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.setAutoReconnect(true);

  const wl_status_t started = password[0] != '\0' ? WiFi.begin(ssid, password) : WiFi.begin(ssid);
  if (started == WL_CONNECT_FAILED) {
    failJoin("RADIO DID NOT START");
    return;
  }
  currentState = State::Joining;
}

void turnOff() {
  stopRadio();
  currentState = State::Off;
}

bool update() {
  const uint32_t now = millis();

  switch (currentState) {
    case State::Scanning:
      return updateScan();
    case State::Joining:
      return updateJoin(now);
    case State::Connected: {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connection lost");
        currentState = State::Lost;
        return true;
      }
      return updateInternet(now);
    }
    case State::Lost:
      if (WiFi.status() != WL_CONNECTED) return false;
      Serial.println("Connection back");
      currentState = State::Connected;
      return true;
    case State::Off:
    case State::ScanDone:
    case State::Failed:
      return false;
  }
  return false;
}

State state() { return currentState; }

ScanEntry* results(int& count) {
  count = scanCount;
  return scanResults;
}

const char* ssid() { return currentSsid; }

void ipText(char* out, int size) {
  const IPAddress ip = WiFi.localIP();
  snprintf(out, size, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

int rssi() { return WiFi.RSSI(); }

const char* failureText() { return failure; }

Internet internet() { return currentInternet; }

}  // namespace network

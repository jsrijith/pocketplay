#pragma once

#include <cstdint>

// The Wi-Fi radio: scanning, joining and the internet time check. It knows nothing
// about the screen or the card. Nothing here waits for the network, so loop() keeps
// reading input, apart from a brief pause while the radio starts or stops.
//
// The radio is only on while scanning or using a network. A finished scan and a
// failed join both leave it off.
namespace network {

constexpr int MAX_SCAN_RESULTS = 24;
constexpr int MAX_SSID_BYTES = 32;

enum class State { Off, Scanning, ScanDone, Joining, Connected, Failed, Lost };

enum class Internet { Waiting, Reached, NotReached };

struct ScanEntry {
  char ssid[MAX_SSID_BYTES + 1];
  int8_t rssi;  // dBm
  bool locked;  // needs a password
};

void startScan();
void join(const char* ssid, const char* password, uint32_t timeoutMs);
void turnOff();  // safe from any state

// Call once per loop. Returns true if anything a screen might show has changed.
bool update();

State state();

// The results of the last scan. Not const, so a screen can sort them for display.
ScanEntry* results(int& count);

const char* ssid();  // the network last asked to join
void ipText(char* out, int size);
int rssi();
const char* failureText();  // why the last join failed

// Whether a time server has answered during this connection. The clock alone can't
// show this, because it may have kept running through sleep.
Internet internet();

}  // namespace network

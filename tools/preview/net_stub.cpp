// The radio and downloads, standing still: Wi-Fi is never connected, so a game that
// offers a download shows its Wi-Fi step and goes no further. Only the drawing is
// being previewed. The clock counts as set, as the PC's is.
#include <cstdio>

#include "core/clock.h"
#include "core/download.h"
#include "core/network.h"

namespace network {

void startScan() {}
void join(const char*, const char*, uint32_t) {}
void turnOff() {}
bool update() { return false; }
State state() { return State::Off; }
ScanEntry* results(int& count) {
  count = 0;
  return nullptr;
}
const char* ssid() { return ""; }
void ipText(char* out, int size) {
  if (size > 0) snprintf(out, static_cast<size_t>(size), "0.0.0.0");
}
int rssi() { return 0; }
const char* failureText() { return ""; }
Internet internet() { return Internet::Waiting; }

}  // namespace network

namespace download {

bool start(const char*, const Sink&, const Options&) { return false; }
State state() { return State::Idle; }
int httpStatus() { return 0; }
void requestCancel() {}
void stopForSleep() {}

}  // namespace download

namespace wallClock {

bool isSet() { return true; }

}  // namespace wallClock

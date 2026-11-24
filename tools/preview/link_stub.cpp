// The two-device radio, standing still: it never starts, so a game's PLAY NEARBY
// shows what it shows when the radio won't, and nobody is ever found. The rest of
// core/link is the real code. The device name is a fixed one.
#include "core/link/name.h"
#include "core/link/radio.h"

namespace linkplay {

namespace {
void (*goodbyeForSleep)() = nullptr;
}  // namespace

Radio::~Radio() { end(); }
bool Radio::begin() { return false; }
void Radio::end() { started_ = false; }
bool Radio::send(const Address&, const uint8_t*, size_t) { return false; }
bool Radio::receive(Address&, uint8_t*, size_t, size_t&) { return false; }
void Radio::enqueueFromCallback(const uint8_t*, const uint8_t*, int) {}

void setBeforeSleep(void (*goodbye)()) { goodbyeForSleep = goodbye; }
void stopForSleep() {
  if (goodbyeForSleep != nullptr) goodbyeForSleep();
  goodbyeForSleep = nullptr;
}

const char* deviceName() { return "STICKY 0000"; }

}  // namespace linkplay

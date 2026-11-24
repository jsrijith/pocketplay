// A Radio that never starts, for the PC. The tests hand PlayBase a FakeLink, so this
// is only here because PlayBase holds a Radio as a member; core/link/radio.cpp needs
// ESP-NOW and only builds for the device.
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

}  // namespace linkplay

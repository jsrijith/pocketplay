#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

// The name this device shows to the other player, such as "STICKY 3F2A": the last
// two bytes of its Wi-Fi MAC, which ESP-NOW also uses as its address. Nothing to set
// up and nothing saved, and two Stickies side by side get different names.
namespace linkplay {

constexpr size_t kDeviceNameBytes = 12;  // "STICKY 3F2A" and the terminator

// Freestanding, for the PC tests.
inline void formatDeviceName(const uint8_t mac[6], char (&out)[kDeviceNameBytes]) {
  snprintf(out, sizeof(out), "STICKY %02X%02X", mac[4], mac[5]);
}

// Reads the MAC from the chip's eFuses, so the radio doesn't need to be on.
const char* deviceName();

}  // namespace linkplay

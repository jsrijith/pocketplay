#pragma once

// The ESP-NOW Transport that a match runs on. Adapted from CrossPlay's
// src/apps_local/link/LinkRadio.h (MIT, see core/LICENSE-CrossPlay.txt): the ESP-NOW
// half only, with no simulator sockets, and the radio taken from core/network.cpp
// instead of from CrossPlay's Developer Mode.
//
// ESP-NOW sends single packets of up to 250 bytes straight to another chip's MAC,
// with no access point, no router and no IP address. That's what lets two devices
// find each other with nothing to set up. It uses the same radio as Wi-Fi, so a
// match and a Wi-Fi connection can't run at once: begin() turns Wi-Fi off.
//
// Device only. The PC tests use tests/link/FakeLink.h instead.

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "transport.h"

namespace linkplay {

// Both devices have to be on the same channel to hear each other, and sweeping all
// thirteen would make finding a player slow. So it's fixed, as CrossPlay's is, and
// must stay the same for the two firmwares to play each other.
constexpr uint8_t kChannel = 1;

// While a match is on, the radio sleeps between listening windows this long. The
// panel takes about half a second to redraw, so the delay this adds can't be seen.
constexpr uint16_t kWakeWindowMs = 50;

class Radio final : public Transport {
 public:
  Radio() = default;
  ~Radio() override;
  Radio(const Radio&) = delete;
  Radio& operator=(const Radio&) = delete;

  // Takes the radio: turns Wi-Fi off, then starts ESP-NOW on kChannel. Returns false,
  // with the radio off, if it can't, including while a download is using Wi-Fi.
  bool begin();
  // Lets any packets still being sent go out, then turns the radio off. Safe to call
  // more than once, and when begin() was never called.
  void end();
  bool started() const { return started_; }

  // True once a packet was dropped because the receive queue was full. Only for the
  // log: the session sends again, so a drop costs time, not state.
  bool overflowed() const { return overflowed_.load(std::memory_order_relaxed); }

  bool send(const Address& to, const uint8_t* data, size_t length) override;
  bool receive(Address& from, uint8_t* buffer, size_t capacity, size_t& length) override;
  const Address& localAddress() const override { return local_; }

  // Called only from ESP-NOW's callbacks, which run on the Wi-Fi task.
  void enqueueFromCallback(const uint8_t* sourceMac, const uint8_t* data, int length);
  void sendFinishedFromCallback() { inFlight_.fetch_sub(1, std::memory_order_relaxed); }

 private:
  // A handful of packets is more than a turn-based game makes between two passes of
  // loop(), and the whole ring is about 1.3 KB.
  static constexpr size_t kQueueDepth = 6;

  struct Datagram {
    Address from;
    uint16_t length = 0;
    uint8_t data[kMaxPacketBytes] = {};
  };

  // A ring with one writer and one reader. The writer is the receive callback, on
  // the Wi-Fi task; the reader is loop(). Each index is written by only one of them,
  // so they need no lock, and the card and panel are still only touched by loop().
  Datagram ring_[kQueueDepth] = {};
  std::atomic<uint8_t> head_{0};  // loop() owns
  std::atomic<uint8_t> tail_{0};  // the callback owns

  // Packets handed to esp_now_send() and not yet reported sent. end() waits for these,
  // so a goodbye sent just before sleep isn't cut off.
  std::atomic<int> inFlight_{0};

  Address local_ = {};
  bool started_ = false;
  // Set once esp_now_init() succeeds, apart from started_, so a begin() that fails
  // halfway still tears ESP-NOW down and the next begin() isn't refused.
  bool espNowUp_ = false;
  // Set once this Radio has changed the Wi-Fi mode, so end() only turns off a radio
  // it took.
  bool held_ = false;
  std::atomic<bool> overflowed_{false};
};

// Sleep, from shell::beforeSleep(). Whatever is using the radio sets a goodbye, which
// tells the other device we're going (Session::leave()) and ends its Radio. Games and
// apps are static objects, so a plain function is enough. nullptr clears it.
void setBeforeSleep(void (*goodbye)());
void stopForSleep();

}  // namespace linkplay

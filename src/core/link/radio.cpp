// Adapted from CrossPlay's src/apps_local/link/LinkRadio.cpp (MIT, see
// core/LICENSE-CrossPlay.txt). See radio.h for what differs.
#include "radio.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <cstring>

#include "core/download.h"
#include "core/network.h"

namespace linkplay {
namespace {

// ESP-NOW's callbacks are bare C functions with no room for a pointer back to us, so
// the radio in use is kept here. Only one match runs at a time.
Radio* activeRadio = nullptr;

void (*beforeSleepGoodbye)() = nullptr;

// How long end() waits for packets still being sent. A send takes about a
// millisecond, so this is only reached if the radio is in trouble.
constexpr uint32_t kFlushTimeoutMs = 50;

void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, const int length) {
  if (activeRadio != nullptr && info != nullptr) activeRadio->enqueueFromCallback(info->src_addr, data, length);
}

void onSent(const esp_now_send_info_t* /*info*/, esp_now_send_status_t /*status*/) {
  if (activeRadio != nullptr) activeRadio->sendFinishedFromCallback();
}

bool addPeer(const Address& address) {
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, address.bytes, sizeof(address.bytes));
  peer.channel = kChannel;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  const esp_err_t added = esp_now_add_peer(&peer);
  return added == ESP_OK || added == ESP_ERR_ESPNOW_EXIST;
}

}  // namespace

Radio::~Radio() { end(); }

void Radio::enqueueFromCallback(const uint8_t* sourceMac, const uint8_t* data, const int length) {
  if (sourceMac == nullptr || data == nullptr) return;
  if (length <= 0 || static_cast<size_t>(length) > kMaxPacketBytes) return;

  const uint8_t tail = tail_.load(std::memory_order_relaxed);
  const uint8_t next = static_cast<uint8_t>((tail + 1) % kQueueDepth);
  if (next == head_.load(std::memory_order_acquire)) {
    overflowed_.store(true, std::memory_order_relaxed);
    return;
  }

  Datagram& slot = ring_[tail];
  memcpy(slot.from.bytes, sourceMac, sizeof(slot.from.bytes));
  memcpy(slot.data, data, static_cast<size_t>(length));
  slot.length = static_cast<uint16_t>(length);
  // Release: the slot's bytes are written before loop() can see the new tail.
  tail_.store(next, std::memory_order_release);
}

bool Radio::begin() {
  end();

  // A download's task is using Wi-Fi, and pulling the radio from under it would
  // fail it halfway. The player can wait for it to finish.
  if (download::state() == download::State::Running) {
    Serial.println("Link: not starting while a download runs");
    return false;
  }
  // ESP-NOW has to be on kChannel, but a Wi-Fi connection keeps the radio on its
  // router's channel. So a match ends any connection or scan first.
  network::turnOff();

  held_ = true;
  WiFi.persistent(false);  // never let the driver store anything in flash
  if (!WiFi.mode(WIFI_STA)) {
    Serial.println("Link: radio would not start");
    end();
    return false;
  }
  // Otherwise the Arduino core may rejoin a network by itself, which moves the radio
  // off kChannel in the middle of a match.
  WiFi.setAutoReconnect(false);
  // Disconnecting is asynchronous, and a channel set while still joined is quietly
  // undone. The timeout waits until it has really happened.
  if (!WiFi.disconnect(false, false, 500)) {
    Serial.println("Link: radio would not leave its network");
    end();
    return false;
  }
  if (esp_wifi_set_channel(kChannel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.printf("Link: could not set channel %u\n", kChannel);
    end();
    return false;
  }
  if (esp_now_init() != ESP_OK) {
    Serial.println("Link: esp_now_init failed");
    end();
    return false;
  }
  espNowUp_ = true;

  activeRadio = this;
  if (esp_now_register_recv_cb(&onReceive) != ESP_OK || esp_now_register_send_cb(&onSent) != ESP_OK) {
    Serial.println("Link: could not register callbacks");
    end();
    return false;
  }
  // ESP-NOW only sends to registered peers, even for the broadcast address that
  // finding a player uses.
  if (!addPeer(kBroadcast)) {
    Serial.println("Link: could not add the broadcast peer");
    end();
    return false;
  }

  // Sleep between short listening windows, rather than listening all the time.
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  esp_now_set_wake_window(kWakeWindowMs);

  if (esp_wifi_get_mac(WIFI_IF_STA, local_.bytes) != ESP_OK) {
    Serial.println("Link: could not read the MAC");
    end();
    return false;
  }

  started_ = true;
  Serial.printf("Link: radio up on channel %u as %02X:%02X:%02X:%02X:%02X:%02X\n", kChannel, local_.bytes[0],
                local_.bytes[1], local_.bytes[2], local_.bytes[3], local_.bytes[4], local_.bytes[5]);
  return true;
}

void Radio::end() {
  if (espNowUp_) {
    // A goodbye is sent just before this, and ESP-NOW only queues a send. Turning
    // the radio off at once could lose it, so wait for the queue to empty.
    const uint32_t startMs = millis();
    while (inFlight_.load(std::memory_order_relaxed) > 0 && millis() - startMs < kFlushTimeoutMs) delay(1);

    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
    esp_wifi_set_ps(WIFI_PS_NONE);
    espNowUp_ = false;
  }
  if (activeRadio == this) activeRadio = nullptr;
  if (started_) {
    Serial.printf("Link: radio off%s\n", overflowed() ? " (the receive queue overflowed)" : "");
  }
  started_ = false;
  head_.store(0, std::memory_order_relaxed);
  tail_.store(0, std::memory_order_relaxed);
  inFlight_.store(0, std::memory_order_relaxed);
  overflowed_.store(false, std::memory_order_relaxed);

  // Only turn off a radio this Radio turned on, because end() also runs when begin()
  // never did, such as from the destructor.
  if (held_) {
    held_ = false;
    WiFi.mode(WIFI_OFF);
    WiFi.setAutoReconnect(true);  // the core's default, which network.cpp expects
  }
}

bool Radio::send(const Address& to, const uint8_t* data, const size_t length) {
  if (!started_ || data == nullptr || length == 0 || length > kMaxPacketBytes) return false;
  // A full peer table fails the send, which the session treats as a lost packet.
  if (!esp_now_is_peer_exist(to.bytes) && !addPeer(to)) return false;

  inFlight_.fetch_add(1, std::memory_order_relaxed);
  if (esp_now_send(to.bytes, data, length) != ESP_OK) {
    inFlight_.fetch_sub(1, std::memory_order_relaxed);
    return false;
  }
  return true;
}

bool Radio::receive(Address& from, uint8_t* buffer, const size_t capacity, size_t& length) {
  if (!started_ || buffer == nullptr) return false;
  while (true) {
    const uint8_t head = head_.load(std::memory_order_relaxed);
    // Acquire: pairs with the callback's release, so the slot is complete.
    if (head == tail_.load(std::memory_order_acquire)) return false;

    const Datagram& slot = ring_[head];
    const bool fits = slot.length <= capacity;
    if (fits) {
      from = slot.from;
      memcpy(buffer, slot.data, slot.length);
      length = slot.length;
    }
    head_.store(static_cast<uint8_t>((head + 1) % kQueueDepth), std::memory_order_release);
    // A packet too big for the caller is dropped, never cut short.
    if (fits) return true;
  }
}

void setBeforeSleep(void (*goodbye)()) { beforeSleepGoodbye = goodbye; }

void stopForSleep() {
  if (beforeSleepGoodbye != nullptr) beforeSleepGoodbye();
  beforeSleepGoodbye = nullptr;
  // In case the goodbye left a radio running.
  if (activeRadio != nullptr) activeRadio->end();
}

}  // namespace linkplay

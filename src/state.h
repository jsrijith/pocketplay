#pragma once

#include <cstddef>
#include <cstdint>

// What survives sleep, and how it turns into bytes for a save file. Values are
// written one at a time rather than copying the struct, so the file layout is a
// decision here instead of whatever the compiler's padding happens to be.

constexpr uint16_t COUNT_LIMIT = 10000;  // wraps to 0, so a count never exceeds 4 digits

constexpr int BUTTON_COUNT = 3;

enum class Swipe : uint8_t { None, Left, Right, Up, Down };

struct Settings {
  uint8_t idleSleepMinutes = 10;  // 1, 5 or 10
};

struct GameState {
  uint16_t buttonCounts[BUTTON_COUNT] = {0, 0, 0};
  uint16_t tapCount = 0;
  uint16_t swipeCount = 0;
  bool hasTapped = false;
  int16_t tapX = 0;
  int16_t tapY = 0;
  Swipe swipe = Swipe::None;
};

constexpr int MAX_SAVED_NETWORKS = 4;
constexpr int MAX_SSID_BYTES = 32;
constexpr int MIN_PASSWORD_BYTES = 8;  // WPA's limits; an open network has no password
constexpr int MAX_PASSWORD_BYTES = 64;

struct SavedNetwork {
  char ssid[MAX_SSID_BYTES + 1] = "";
  char password[MAX_PASSWORD_BYTES + 1] = "";
};

// Most recently used first.
struct SavedNetworks {
  uint8_t count = 0;
  SavedNetwork items[MAX_SAVED_NETWORKS];
};

const SavedNetwork* findNetwork(const SavedNetworks& networks, const char* ssid);

// Adds it or moves it to the front, dropping the oldest if the list is full. Returns
// true if the list changed and needs saving.
bool rememberNetwork(SavedNetworks& networks, const char* ssid, const char* password);

// Returns true if the network was in the list.
bool forgetNetwork(SavedNetworks& networks, const char* ssid);

constexpr uint8_t SETTINGS_VERSION = 1;
constexpr size_t SETTINGS_BYTES = 1;

constexpr uint8_t GAME_VERSION = 1;
constexpr size_t GAME_BYTES = 16;

constexpr uint8_t NETWORKS_VERSION = 1;
constexpr size_t NETWORKS_MAX_BYTES = 1 + MAX_SAVED_NETWORKS * (1 + MAX_SSID_BYTES + 1 + MAX_PASSWORD_BYTES);
constexpr int NETWORK_KEY_BYTES = 6;  // the chip's MAC address

size_t packSettings(const Settings& settings, uint8_t* out);
size_t packGame(const GameState& game, uint8_t* out);

// Passwords are scrambled with the key so they can't be read straight off the card.
// This isn't encryption: anyone with the card and this code can undo it.
size_t packNetworks(const SavedNetworks& networks, const uint8_t* key, uint8_t* out);

// Return false and leave the result alone if the bytes aren't something this
// firmware can trust: too short, a newer version, or a value out of range.
bool unpackSettings(uint8_t version, const uint8_t* in, size_t size, Settings& settings);
bool unpackGame(uint8_t version, const uint8_t* in, size_t size, GameState& game);
bool unpackNetworks(uint8_t version, const uint8_t* in, size_t size, const uint8_t* key, SavedNetworks& networks);

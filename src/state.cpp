#include "state.h"

#include <cstring>

namespace {

bool validIdleMinutes(uint8_t minutes) { return minutes == 1 || minutes == 5 || minutes == 10; }

bool validPasswordLength(size_t length) { return length == 0 || (length >= MIN_PASSWORD_BYTES && length <= MAX_PASSWORD_BYTES); }

int networkIndex(const SavedNetworks& networks, const char* ssid) {
  for (int i = 0; i < networks.count; i++) {
    if (strcmp(networks.items[i].ssid, ssid) == 0) return i;
  }
  return -1;
}

}  // namespace

const SavedNetwork* findNetwork(const SavedNetworks& networks, const char* ssid) {
  const int index = networkIndex(networks, ssid);
  return index < 0 ? nullptr : &networks.items[index];
}

bool rememberNetwork(SavedNetworks& networks, const char* ssid, const char* password) {
  const size_t ssidLength = strlen(ssid);
  const size_t passwordLength = strlen(password);
  if (ssidLength == 0 || ssidLength > MAX_SSID_BYTES || !validPasswordLength(passwordLength)) return false;

  int index = networkIndex(networks, ssid);
  if (index == 0 && strcmp(networks.items[0].password, password) == 0) return false;

  if (index < 0) {
    if (networks.count < MAX_SAVED_NETWORKS) networks.count++;
    index = networks.count - 1;
  }

  for (int i = index; i > 0; i--) {
    networks.items[i] = networks.items[i - 1];
  }
  memcpy(networks.items[0].ssid, ssid, ssidLength + 1);
  memcpy(networks.items[0].password, password, passwordLength + 1);
  return true;
}

bool forgetNetwork(SavedNetworks& networks, const char* ssid) {
  const int index = networkIndex(networks, ssid);
  if (index < 0) return false;

  for (int i = index; i + 1 < networks.count; i++) {
    networks.items[i] = networks.items[i + 1];
  }
  networks.count--;
  networks.items[networks.count] = SavedNetwork{};
  return true;
}

size_t packNetworks(const SavedNetworks& networks, const uint8_t* key, uint8_t* out) {
  uint8_t* cursor = out;
  *cursor++ = networks.count;
  for (int n = 0; n < networks.count; n++) {
    const SavedNetwork& network = networks.items[n];

    const size_t ssidLength = strlen(network.ssid);
    *cursor++ = static_cast<uint8_t>(ssidLength);
    memcpy(cursor, network.ssid, ssidLength);
    cursor += ssidLength;

    const size_t passwordLength = strlen(network.password);
    *cursor++ = static_cast<uint8_t>(passwordLength);
    for (size_t i = 0; i < passwordLength; i++) {
      *cursor++ = static_cast<uint8_t>(network.password[i]) ^ key[i % NETWORK_KEY_BYTES];
    }
  }
  return static_cast<size_t>(cursor - out);
}

bool unpackNetworks(uint8_t version, const uint8_t* in, size_t size, const uint8_t* key, SavedNetworks& networks) {
  if (version != NETWORKS_VERSION || size < 1) return false;

  SavedNetworks loaded;
  const uint8_t* cursor = in;
  const uint8_t* const end = in + size;

  const uint8_t count = *cursor++;
  if (count > MAX_SAVED_NETWORKS) return false;

  for (int n = 0; n < count; n++) {
    SavedNetwork& network = loaded.items[n];

    if (cursor == end) return false;
    const size_t ssidLength = *cursor++;
    if (ssidLength == 0 || ssidLength > MAX_SSID_BYTES || ssidLength > static_cast<size_t>(end - cursor)) return false;
    memcpy(network.ssid, cursor, ssidLength);
    network.ssid[ssidLength] = '\0';
    if (strlen(network.ssid) != ssidLength) return false;
    cursor += ssidLength;

    if (cursor == end) return false;
    const size_t passwordLength = *cursor++;
    if (!validPasswordLength(passwordLength) || passwordLength > static_cast<size_t>(end - cursor)) return false;
    for (size_t i = 0; i < passwordLength; i++) {
      network.password[i] = static_cast<char>(*cursor++ ^ key[i % NETWORK_KEY_BYTES]);
    }
    network.password[passwordLength] = '\0';
    if (strlen(network.password) != passwordLength) return false;
  }
  if (cursor != end) return false;

  loaded.count = count;
  networks = loaded;
  return true;
}

size_t packSettings(const Settings& settings, uint8_t* out) {
  out[0] = settings.idleSleepMinutes;
  return SETTINGS_BYTES;
}

bool unpackSettings(uint8_t version, const uint8_t* in, size_t size, Settings& settings) {
  if (version != SETTINGS_VERSION || size != SETTINGS_BYTES) return false;
  if (!validIdleMinutes(in[0])) return false;

  settings.idleSleepMinutes = in[0];
  return true;
}

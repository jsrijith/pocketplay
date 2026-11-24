#include "name.h"

#include <esp_mac.h>

namespace linkplay {

const char* deviceName() {
  static char name[kDeviceNameBytes] = {};
  if (name[0] == '\0') {
    uint8_t mac[6] = {};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
      formatDeviceName(mac, name);
    } else {
      snprintf(name, sizeof(name), "STICKY");
    }
  }
  return name;
}

}  // namespace linkplay

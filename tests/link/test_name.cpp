// The device name, made from the MAC (core/link/name.h).
#include <cstdio>
#include <cstring>

#include "core/link/name.h"
#include "core/link/protocol.h"

static_assert(linkplay::kDeviceNameBytes - 1 <= linkplay::kMaxNameBytes,
              "the name must fit the protocol's name field, or the other device shows it cut short");

int main() {
  int failed = 0;
  char name[linkplay::kDeviceNameBytes];

  const uint8_t mac[6] = {0x24, 0x58, 0x7C, 0x01, 0x3F, 0x2A};
  linkplay::formatDeviceName(mac, name);
  if (strcmp(name, "STICKY 3F2A") != 0) {
    printf("FAIL  expected STICKY 3F2A, got %s\n", name);
    failed++;
  }

  const uint8_t zeros[6] = {};
  linkplay::formatDeviceName(zeros, name);
  if (strcmp(name, "STICKY 0000") != 0) {
    printf("FAIL  expected STICKY 0000, got %s\n", name);
    failed++;
  }

  if (failed > 0) return 1;
  printf("link name: all passed\n");
  return 0;
}

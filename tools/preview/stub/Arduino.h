#pragma once
// Just enough of Arduino for drawing and game code to compile on the PC.
#include <cstdio>

struct PreviewSerial {
  template <typename... Args>
  void printf(const char* format, Args... args) {
    std::printf(format, args...);
  }
  void println(const char* text) { std::printf("%s\n", text); }
};
inline PreviewSerial Serial;

inline unsigned long micros() { return 0; }
inline unsigned long millis() { return 0; }
// FreeRTOS: the least stack the current task has had free, in bytes on the ESP32.
inline unsigned uxTaskGetStackHighWaterMark(void*) { return 0; }

#pragma once

// The log calls in the files copied from CrossPlay, which has its own Logging.h. On
// the device they print to the serial port; in the PC tests they compile away.
#if defined(ARDUINO)
#include <Arduino.h>
#define LOG_INF(tag, format, ...) Serial.printf("Link: " format "\n", ##__VA_ARGS__)
#define LOG_ERR(tag, format, ...) Serial.printf("Link error: " format "\n", ##__VA_ARGS__)
#else
#define LOG_INF(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#endif

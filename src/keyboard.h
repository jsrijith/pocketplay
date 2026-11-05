#pragma once

#include <cstdint>

// An on-screen keyboard for typing a Wi-Fi password. It covers all printable ASCII,
// shows what's typed, and only offers JOIN once the text is long enough. It knows
// nothing about Wi-Fi beyond those length limits.
namespace keyboard {

constexpr int MIN_LENGTH = 8;   // WPA's shortest password
constexpr int MAX_LENGTH = 64;  // WPA's longest password

enum class Event { None, Changed, Back, Join };

// Starts empty. The title names what's being typed and is copied.
void begin(const char* title);
const char* text();

Event onTap(int x, int y);
void draw(uint8_t* frame);

}  // namespace keyboard

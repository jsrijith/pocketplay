#pragma once
// The chip's random numbers, fixed on the PC so a preview deals the same game every run.
#include <cstdint>

inline uint32_t esp_random() { return 0x5EED1234u; }

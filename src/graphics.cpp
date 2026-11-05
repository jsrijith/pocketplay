#include "graphics.h"

// Framebuffer format: 1 bit per pixel, most significant bit first, 1 = white.
void setPixel(uint8_t* frame, int x, int y, bool black) {
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;

  const int panelX = y;
  const int panelY = SCREEN_WIDTH - 1 - x;
  uint8_t& byte = frame[panelY * EInkDisplay::DISPLAY_WIDTH_BYTES + panelX / 8];
  const auto mask = static_cast<uint8_t>(0x80 >> (panelX % 8));
  if (black) {
    byte &= static_cast<uint8_t>(~mask);
  } else {
    byte |= mask;
  }
}

void fillRect(uint8_t* frame, int left, int top, int width, int height, bool black) {
  for (int y = top; y < top + height; y++) {
    for (int x = left; x < left + width; x++) {
      setPixel(frame, x, y, black);
    }
  }
}

void drawRectOutline(uint8_t* frame, int left, int top, int width, int height, int thickness) {
  fillRect(frame, left, top, width, thickness, true);
  fillRect(frame, left, top + height - thickness, width, thickness, true);
  fillRect(frame, left, top, thickness, height, true);
  fillRect(frame, left + width - thickness, top, thickness, height, true);
}

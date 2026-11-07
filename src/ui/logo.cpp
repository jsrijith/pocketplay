#include "ui/logo.h"

#include <cmath>
#include <cstdio>

#include "core/graphics.h"
#include "core/text.h"
#include "core/version.h"

namespace ui {

namespace {

// A convex polygon, filled or cleared, by testing each pixel against every edge.
void polygon(uint8_t* frame, const float* xs, const float* ys, int n, bool black) {
  float left = xs[0], right = xs[0], top = ys[0], bottom = ys[0];
  for (int k = 1; k < n; k++) {
    left = std::fmin(left, xs[k]);
    right = std::fmax(right, xs[k]);
    top = std::fmin(top, ys[k]);
    bottom = std::fmax(bottom, ys[k]);
  }
  for (int y = static_cast<int>(top); y <= static_cast<int>(bottom) + 1; y++) {
    for (int x = static_cast<int>(left); x <= static_cast<int>(right) + 1; x++) {
      const float px = x + 0.5f;
      const float py = y + 0.5f;
      int ahead = 0;
      int behind = 0;
      for (int k = 0; k < n; k++) {
        const int j = (k + 1) % n;
        const float e = (xs[j] - xs[k]) * (py - ys[k]) - (ys[j] - ys[k]) * (px - xs[k]);
        ahead += e >= 0;
        behind += e <= 0;
      }
      if (ahead == n || behind == n) setPixel(frame, x, y, black);
    }
  }
}

// A straight stroke `width` thick with square ends.
void stroke(uint8_t* frame, float x0, float y0, float x1, float y1, float width) {
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float length = std::sqrt(dx * dx + dy * dy);
  const float nx = -dy / length * width / 2;
  const float ny = dx / length * width / 2;
  const float xs[4] = {x0 + nx, x1 + nx, x1 - nx, x0 - nx};
  const float ys[4] = {y0 + ny, y1 + ny, y1 - ny, y0 - ny};
  polygon(frame, xs, ys, 4, true);
}

// Stitching: short dashes along a line.
void stitch(uint8_t* frame, float x0, float y0, float x1, float y1) {
  constexpr float DASH = 12;
  constexpr float SPACE = 9;
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float length = std::sqrt(dx * dx + dy * dy);
  for (float s = 0; s < length; s += DASH + SPACE) {
    const float e = std::fmin(s + DASH, length);
    stroke(frame, x0 + dx * s / length, y0 + dy * s / length, x0 + dx * e / length, y0 + dy * e / length, 4);
  }
}

}  // namespace

void drawLogo(uint8_t* frame, int centerX, int top) {
  const float cx = static_cast<float>(centerX);
  const float t = static_cast<float>(top);
  const float opening = t + 138;  // the pocket's top edge

  // The die first, tipped a little, so the pocket hides its lower part.
  const float dieX = cx + 10;
  const float dieY = t + 100;
  constexpr float HALF = 66;
  constexpr float ANGLE = -0.22f;
  const float c = std::cos(ANGLE);
  const float s = std::sin(ANGLE);
  auto place = [&](float u, float v, float& x, float& y) {
    x = dieX + u * c - v * s;
    y = dieY + u * s + v * c;
  };
  float xs[5];
  float ys[5];
  const float corners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
  for (int k = 0; k < 4; k++) place(corners[k][0] * HALF, corners[k][1] * HALF, xs[k], ys[k]);
  polygon(frame, xs, ys, 4, true);
  constexpr float INNER = HALF - 9;
  for (int k = 0; k < 4; k++) place(corners[k][0] * INNER, corners[k][1] * INNER, xs[k], ys[k]);
  polygon(frame, xs, ys, 4, false);
  const float pips[5][2] = {{-30, -30}, {30, -30}, {0, 0}, {-30, 30}, {30, 30}};
  for (const auto& p : pips) {
    float x;
    float y;
    place(p[0], p[1], x, y);
    drawRing(frame, static_cast<int>(x), static_cast<int>(y), 11, 11, true);
  }

  // The pocket: cleared inside, so it covers the die, then its edge, rounded at the corners.
  const float px[5] = {cx - 130, cx + 130, cx + 130, cx, cx - 130};
  const float py[5] = {opening, opening, opening + 210, opening + 270, opening + 210};
  const float bodyX[4] = {px[0], px[1], px[2], px[4]};
  const float bodyY[4] = {py[0], py[1], py[2], py[4]};
  polygon(frame, bodyX, bodyY, 4, false);
  const float pointX[3] = {px[4], px[2], px[3]};
  const float pointY[3] = {py[4], py[2], py[3]};
  polygon(frame, pointX, pointY, 3, false);
  for (int k = 0; k < 5; k++) stroke(frame, px[k], py[k], px[(k + 1) % 5], py[(k + 1) % 5], 10);
  for (int k = 0; k < 5; k++) drawRing(frame, static_cast<int>(px[k]), static_cast<int>(py[k]), 5, 5, true);

  // Stitching just inside the edge.
  const float sx[5] = {cx - 110, cx + 110, cx + 110, cx, cx - 110};
  const float sy[5] = {opening + 26, opening + 26, opening + 196, opening + 246, opening + 196};
  for (int k = 0; k < 5; k++) stitch(frame, sx[k], sy[k], sx[(k + 1) % 5], sy[(k + 1) % 5]);
}

void formatVersion(char* out, int size) {
  char commit[16];
  std::snprintf(commit, sizeof(commit), "%s", version::COMMIT);
  for (char* c = commit; *c; c++) {
    if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
  }
  std::snprintf(out, static_cast<size_t>(size), "VERSION %s  BUILD %d (%s)", version::NUMBER, version::BUILD, commit);
}

void drawSplash(uint8_t* frame, const char* line) {
  constexpr int LOGO_TOP = 150;
  drawLogo(frame, SCREEN_WIDTH / 2, LOGO_TOP);
  const int nameTop = LOGO_TOP + LOGO_HEIGHT + 30;
  drawTextInBox(frame, Font::Size44, 0, nameTop, SCREEN_WIDTH, 60, "POCKETPLAY");
  if (line) drawTextInBox(frame, Font::Size20, 0, nameTop + 66, SCREEN_WIDTH, 30, line);
  char text[64];
  formatVersion(text, sizeof(text));
  drawTextInBox(frame, Font::Size14, 0, SCREEN_HEIGHT - 16 - 24, SCREEN_WIDTH, 24, text);
}

}  // namespace ui

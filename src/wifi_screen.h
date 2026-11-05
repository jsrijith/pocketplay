#pragma once

#include <cstddef>
#include <cstdint>

// The Wi-Fi screens: the network list, the password keyboard and the status page,
// and the steps between them. It also keeps the remembered networks on the card.
namespace wifiScreen {

// Reads the remembered networks. Call after storage::begin().
void begin();

// Call once per loop, whether or not the screen is open. Returns true if the
// display needs redrawing.
bool update();

// Opens the screen and takes the next step: joins the most recent network if the
// radio is off and one is remembered, scans if none is, or shows the status.
void open();
bool isOpen();

// These return true if the display needs redrawing.
bool onTap(int x, int y);
bool onUpPressed();
bool onDownPressed();

void draw(uint8_t* frame);

// The input screen's one-line summary, such as "WIFI OFF".
void statusLine(char* out, size_t size, int maxWidth);

}  // namespace wifiScreen

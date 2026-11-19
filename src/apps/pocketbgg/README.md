# PocketBGG

Your BoardGameGeek collection, to browse on the device.

**Work in progress.** It can't sync yet, so it shows sample games, marked `SAMPLE DATA` at the top. Five of them are real BGG games, chosen to test long names and unusual years; the rest are made up.

## Getting around

- The list shows each game's name, with the year and how many times it's been played under it. Long names wrap onto a second line rather than being cut short.
- The Up and Down buttons turn the pages. `PAGE 1 OF 4` at the top says where you are.
- `SYNC` will download your collection. For now it tests that the device checks BGG's certificate: it joins Wi-Fi, opening the Wi-Fi screen if it isn't already connected, asks BGG for something small twice with no token, and turns the radio off again. The line above it should read `BGG ROOT: HTTP 401. WRONG ROOT: REFUSED`: BGG answered when its real root was trusted, and a wrong root was refused. `BACK` while it's checking stops it first.
- Tap a game to see its details. `BACK` returns to the same page of the list.
- `BACK` on the list returns to the APPS page.

## Saves

Nothing is saved yet.

# core/link: two-device play

Games between two Stickies over ESP-NOW, with no router and no setup. The files CrossPlay names below come from its `src/apps_local/link/` (MIT), unchanged apart from includes, and they stay wire-compatible with it, so a PocketPlay device can play a CrossPlay device.

| File | CrossPlay's name | Holds |
|---|---|---|
| `protocol.{h,cpp}` | `LinkProtocol` | The packet format: `XLNK`, version 1, a 12-byte header and up to 192 bytes of payload |
| `transport.h` | `LinkTransport.h` | What the session needs from a radio: send, receive, this device's address |
| `session.{h,cpp}` | `LinkSession` | Finding a player, retries, heartbeat, timeout and Bye |
| `play.{h,cpp}` | `LinkPlay` | `Play<State>`, what a game holds: phases, `play()`, `takeOpponent()`, notes, and the table of game IDs |
| `endgame.h` | `LinkEndgame.h` | Holds the final board for a moment before ANOTHER GAME? |
| `match.{h,cpp}` | from `LinkActivity` | PocketPlay's: the part of a match that's the same in every game, without screens. `match.h` lists what a game calls |
| `radio.{h,cpp}` | from `LinkRadio` | PocketPlay's: ESP-NOW on channel 1, taking the radio from Wi-Fi. Device only |
| `name.{h,cpp}` | | PocketPlay's: this device's name, such as `STICKY 3F2A`, from its MAC |
| `log.h` | | Prints the copied files' log lines to the serial port |

`ui/link/link_screens` draws the match screen (LOOKING FOR A PLAYER, the two seats, PLAY AGAIN) for every game.

Everything here except the radio and the name is freestanding: there's no Arduino in it, and time comes in as an argument. That's why `tests/link/` can play whole matches on the PC over `FakeLink.h`, which drops, delays, duplicates and reorders packets on purpose, with `radio_stub.cpp` standing in for the radio.

## Compatibility with CrossPlay

The packet format, the radio channel, the game IDs and each game's shared state are the protocol. Changing any of them makes PocketPlay and CrossPlay stop finding each other, or, worse, pair and then misread each other's states. So keep them as CrossPlay has them. A game that only PocketPlay has takes an ID in `0xE0xx`, and so does a ported game whose state PocketPlay changes.

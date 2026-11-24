# core/link: two-device play

Games between two Stickies over ESP-NOW, with no router and no setup. The code comes from CrossPlay's `src/apps_local/link/` (MIT), unchanged apart from includes, and stays wire-compatible with it, so a PocketPlay device can play a CrossPlay device.

| File | CrossPlay's name | Holds |
|---|---|---|
| `protocol.{h,cpp}` | `LinkProtocol` | The packet format: `XLNK`, version 1, a 12-byte header and up to 192 bytes of payload |
| `transport.h` | `LinkTransport.h` | What the session needs from a radio: send, receive, this device's address |
| `session.{h,cpp}` | `LinkSession` | Finding a player, retries, heartbeat, timeout and Bye |

Everything here except the radio is freestanding: there's no Arduino in it, and time comes in as an argument. That's why `tests/link/` can play whole matches on the PC over `FakeLink.h`, which drops, delays, duplicates and reorders packets on purpose.

## Compatibility with CrossPlay

The packet format, the radio channel, the game IDs and each game's shared state are the protocol. Changing any of them makes PocketPlay and CrossPlay stop finding each other, or, worse, pair and then misread each other's states. So keep them as CrossPlay has them. A game that only PocketPlay has takes an ID in `0xE0xx`, and so does a ported game whose state PocketPlay changes.

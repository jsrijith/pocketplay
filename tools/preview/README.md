# Preview

Draws a game's screens on the PC as PNGs, so a new or ported game can be checked against CrossPlay's screenshots before anything is flashed. It uses the real drawing code (`core/graphics`, `core/text`, `ui/`) and the game's own folder, with a card kept in memory and no panel.

```bash
tools/preview/run.sh FOLDER FUNCTION OUT_DIR COMMAND...
tools/preview/run.sh dungeon dungeonGame /tmp/shots shot:menu tap:240,758 shot:board row:row
```

`FOLDER` is the game's folder under `src/games/`, and `FUNCTION` is the function in `game_list.inc`. The game is opened once, then the commands run in order:

| Command | Does |
|---|---|
| `tap:X,Y` | Taps at a portrait screen position (0..479, 0..799), and prints the `TapResult`. Use it to check that a tap changing nothing returns `Nothing`. |
| `shot:NAME` | Draws the game into `OUT_DIR/NAME.png`. |
| `row:NAME` | Draws the game's home-page row, to check its name and icon. |

CrossPlay's own screenshots are in `/home/srijith/mycrossplay/site/assets/shots/`, at the same 480 x 800.

**Limits:**
- It links `core/graphics`, `core/text`, `ui/` and the game's folder. `net_stub.cpp` stands in for `core/network` and `core/download`: Wi-Fi is never connected, so a download stops at its Wi-Fi step. The clock is the PC's.
- A game's downloaded data can be put on the in-memory card before the game opens. `PREVIEW_BLOB_<name>=<file>` loads a file as the blob `<name>`, with `_` in the name read as `-`:

  ```bash
  PREVIEW_BLOB_connections_idx=tests/connections/archive.idx \
  PREVIEW_BLOB_connections_dat=tests/connections/archive.dat \
    tools/preview/run.sh connections connectionsGame /tmp/shots shot:menu
  ```
- A preview shows pixels, not the panel. Ghosting, refresh timing, touch accuracy and sleep still have to be checked on the device.
- It runs under AddressSanitizer and UBSan, so a memory error in drawing code stops it.

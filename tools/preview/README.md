# Preview

Draws a game's or an app's screens on the PC as PNGs, so they can be checked before anything is flashed. It uses the real drawing code (`core/graphics`, `core/text`, `ui/`) and the game's own folder, with a card kept in memory and no panel.

```bash
tools/preview/run.sh FOLDER FUNCTION OUT_DIR COMMAND...
tools/preview/run.sh dungeon dungeonGame /tmp/shots shot:menu tap:240,758 shot:board row:row
```

`FOLDER` is the game's folder under `src/games/`, and `FUNCTION` is the function in `game_list.inc`. For an app, `FOLDER` is `apps/<folder>` and `FUNCTION` the function in `app_list.inc`:

```bash
tools/preview/run.sh apps/calculator calculatorApp /tmp/shots shot:keys tap:67,632 shot:one
```

The game or app is opened once, then the commands run in order:

| Command | Does |
|---|---|
| `tap:X,Y` | Taps at a screen position (0..479, 0..799 in portrait, or 0..799, 0..479 for a game whose `landscape()` is true), and prints the `TapResult`. Use it to check that a tap changing nothing returns `Nothing`. |
| `shot:NAME` | Draws the game into `OUT_DIR/NAME.png`. |
| `row:NAME` | Draws the game's home-page row, to check its name and icon. |
| `update` | Runs the game's `update()` as the loop does, at least once and then while `busy()` is true, and prints whether it asked for a redraw. For a game with work between taps, such as Sudoku making a puzzle. |
| `swipe:X1,Y1,X2,Y2` | A finger drawn from one screen position to another, through the game's `onSwipe()`, printing the `TapResult`. |
| `up`, `down` | The Up and Down buttons, through the game's `onUpPressed()` and `onDownPressed()`, printing the `TapResult`. |
| `press:X,Y`, `hold:X,Y`, `release` | For a game whose `usesHold()` is true: a finger going down, the same finger held for 400 ms, and the press ending. On the device a hold ends its own press, so follow `hold:` with `release` and no `tap:`. |
| `warning:NAME` | Draws the game with the shell's longest warning, `SAVE FAILED`, where the shell puts it, to check nothing is drawn under it. |

A landscape game's screens come out landscape, and its `row:` stays portrait, as the home page is.

An app gets taps, Up and Down, and `update()`, as on the device, so `press:`, `hold:`, `release` and `swipe:` stop with an error for one. The preview wraps the app as a game, and the results print with a game's names (`Leave` for BACK).

**README screenshots:** `tools/preview/screenshots.sh` draws the screenshot in every game's and app's `README.md`: a screen of it being played, saved as `screenshot.png` in its folder. Each game has one line in the script, with the commands that reach that screen. Run it after changing a screen, for all of them or for named folders (`tools/preview/screenshots.sh sudoku apps/calculator`), and look at what changed before committing. Games that need downloaded or card data get made-up data from `screenshot_data.py`: one Wordle answer, one Connections puzzle, Last Piece's example challenges and Think Alike's starter questions. Sudoku and Grid Gambit make a new puzzle each run, and Wordle and Word Ladder show the PC's date, so those screenshots change every time.

**Limits:**
- It links `core/graphics`, `core/text`, all of `ui/` (the keyboard too) and the game's folder. `net_stub.cpp` stands in for `core/network`, `core/download` and the clock's `isSet()`: Wi-Fi is never connected, so a download stops at its Wi-Fi step, and the clock counts as set. The clock is the PC's.
- A game's downloaded data can be put on the in-memory card before the game opens. `PREVIEW_BLOB_<name>=<file>` loads a file as the blob `<name>`, with `_` in the name read as `-`:

  ```bash
  PREVIEW_BLOB_connections_pack=pack.bin \
    tools/preview/run.sh connections connectionsGame /tmp/shots shot:menu
  ```

  The file has to be in the blob's own format. For Connections that's a 4-byte index length, then `tests/connections/archive.idx`, then `archive.dat`.
- A save can be put there the same way, to preview a game part-way through: `PREVIEW_SAVE_<name>=<file>` loads the file as the save `<name>`, and the file's first byte is the save's version.
- Files the player copies onto the card, read with `storage::listFolder()` and `storage::readFile()`, come from the PC folder named by `PREVIEW_CARD`, so `/textadv` is `$PREVIEW_CARD/textadv`.
- A preview shows pixels, not the panel. Ghosting, refresh timing, touch accuracy and sleep still have to be checked on the device.
- It runs under AddressSanitizer and UBSan, so a memory error in drawing code stops it.

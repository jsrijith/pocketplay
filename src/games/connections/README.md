# Connections

Sixteen words hide four groups of four. Tap four that belong together, then `SUBMIT`. Four wrong guesses end the game. `ONE AWAY` means three of the four were right, and the same four can't cost a mistake twice. `GET PUZZLES` downloads every published puzzle once, and after that playing never touches the network.

- **Menu:** the newest puzzle's date is the way into it, with whether it's not started, in progress or solved. Below are the record, the last sixteen days drawn as a board (solid for a clean solve, grey for a solve with mistakes, a cross for a loss), and `HOW TO PLAY`, `ARCHIVE` and `GET PUZZLES` (`ALL CAUGHT UP` once today's puzzle is on the card). Tapping the sixteen days also opens the archive.
- **Board:** the date is the title. A tap selects a tile, and a selected tile is solid black. `SHUFFLE` moves the tiles but never changes the answer, `CLEAR` drops the selection, and `SUBMIT` is grey until four are picked. A solved group turns into a black row with its name over its words. The four dots are the mistakes left. Winning or losing is a full refresh, and a loss shows every group.
- **Archive:** a month at a time, with year and month steppers that stop at the archive's ends. A clean solve has a sparkle, a solve with mistakes has one dot per mistake, and a loss has a cross. `TODAY` returns to the newest month.
- **Record:** the streak counts back from the newest puzzle on the card, and skips that day while it's unplayed. The first finish of a day is the one kept.

**Saves:** all of these are in `/.pocketplay/`.

| Name | Kind | Holds |
|---|---|---|
| `connections` | Save | The puzzle being played: which one, its shuffle, the groups found, mistakes, guesses tried and the tiles selected. 35 bytes, written on every change and erased when the game ends. |
| `connections-pack` | Blob | Every puzzle, in CrossPlay's pack format: a 4-byte length, the index, then the puzzle records. About 230 KB, read whole into PSRAM. |
| `connections-results` | Blob | One byte per day from 2023-06-12: played, lost, mistakes, and whether the hardest group went first. |

**Where the puzzles come from:** [Eyefyre/NYT-Connections-Answers](https://github.com/Eyefyre/NYT-Connections-Answers), the same archive CrossPlay uses, in one 1.3 MB download parsed as it arrives. Curly quotes and accents are folded to ASCII, and the two emoji puzzles are skipped. The puzzles are the New York Times'. This firmware ships none of them, and the device fetches them when asked.

**Copied code:** these are CrossPlay's `src/apps_local/connections/` files, each changed only in its includes: `connections_rules.h/.cpp` (`ConnectionsCore`), `connections_pack.h/.cpp` (`ConnectionsPack`), `connections_results.h/.cpp` (`ConnectionsResults`), `connections_import.h/.cpp` (`ConnectionsImport`) and `connections_text.h` (`ConnectionsText.h`). `tests/connections/test_connections.cpp` and `test_pack.cpp` are its tests, and `archive.idx` and `archive.dat` are its `host-tests/tilefit` fixture. `connections_fit.cpp` is the tile and solved-row layout from `ConnectionsScreens.cpp`, moved out so the test can walk every board through it. The screens and game are ported from `ConnectionsScreens.cpp` and `ConnectionsActivity.cpp`, redrawn without CrossPlay's UI toolkit. All of it matched CrossPlay commit `fbeac762` when last compared, and it's under the MIT licence, in `LICENSE-CrossPlay.txt`.

**Differences from CrossPlay:**

- `BACK` is a button in the header, because PocketPlay has no edge swipe. It leaves the board, the archive and how to play for the menu, stops a download, and leaves the game from the menu.
- Everything is set in Jersey 25, because PocketPlay has no serif font. Jersey is wider, so the tiles step through three cuts (Size20, Size14, Size10) where CrossPlay steps through two serif ones. Across the 1,143 boards of the archive, 17 full boards set at Size20, 272 at Size14 and 854 at Size10, and 220 have a word that has to be cut, evenly, across lines (137 in CrossPlay). The rules are CrossPlay's: one cut for the whole board, shrink before breaking a word, and never shorten one. `tests/connections/` checks all of it.
- **Resume:** tapping the puzzle that's in progress picks it up where it was. CrossPlay's menu says `IN PROGRESS`, but tapping it starts the board again.
- **Record:** the first finish of a day is the one kept, as in PocketPlay's Wordle. In CrossPlay, playing a finished day again replaces its result.
- **Saving:** the board saves on every change, including the tiles selected, so a wake comes back to it exactly. CrossPlay saves a line of text when the board is left or finished. The save is packed byte by byte and erased when the game ends.
- **Downloading:** the download runs in its own task, so the screen stays live and `BACK` stops it. CrossPlay blocks for the whole download and restarts the device afterwards. The radio turns off when the download ends.
- **Storage:** the pack is one blob read whole into PSRAM, where CrossPlay seeks two files on the card. Nothing is imported from a CrossPlay card.
- A tap that changes nothing draws nothing: a fifth tile, `CLEAR` with nothing selected, `SUBMIT` with fewer than four, `ALREADY GUESSED` a second time, a spent stepper, a day with no puzzle, `TODAY` on the month already shown, `ALL CAUGHT UP`, and `ARCHIVE` before the first download.
- Opening a board is a full refresh, as starting a game is everywhere in PocketPlay.
- A calendar day's mistake dots are a third of the mark's width rather than a quarter, because a quarter is 6 px in this cell.
- The archive's right label (`N PLAYED`) is Size14, one cut below CrossPlay's, so it doesn't run into `ARCHIVE` beside `BACK`.
- There are no button hints, and CrossPlay's unused archive list view isn't ported.

**Kept from CrossPlay as it is:** a resumed board is dealt again from its seed with the solved groups taken out. After a `SHUFFLE` that came after a group was solved, the tiles can come back from sleep in a different order, though they're the same tiles with the same words selected.

**Icon:** `connections_icon.h` is CrossPlay's home-page icon for this game, drawn from the Lucide `grid-2x2` icon (ISC licence).

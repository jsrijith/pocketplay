# Minesweeper

Eight by ten with ten mines. Tap a cell to dig it: it shows how many mines touch it, or opens its neighbours if that's none. Hold a cell to plant a flag. Dig every safe cell to win, and dig a mine to lose. The first dig is always safe and always opens a space, because the mines are laid after it, away from it and its neighbours.

- **Board:** the cell under your finger gets a heavy border while a hold builds, and a hold of 400 ms plants or lifts a flag. `DIG`/`FLAG` under the board switches what a tap does, and a hold flags in both. Tapping a number that already has all its flags opens the rest of its neighbours (a chord), and a wrong flag there loses. The count says how many mines are left unflagged. A settled board stays, mines bared, with `CLEARED` or `BOOM` under it, which opens the result.
- **Result:** cells opened and mines flagged, then `PLAY AGAIN` or `DONE`.
- **Menu:** the record (played, cleared), the last settled board, and `PLAY` and `HOW TO PLAY`. With a board in play, `PLAY` is `RESUME`, and `NEW GAME` sits under it.
- **How to play:** five lessons, each with a diagram whose numbers are counted from its own mines.

**Saves:** both are in `/.pocketplay/`.

| Name | Kind | Holds |
|---|---|---|
| `minesweeper` | Save | The board in play: its cells, the random state the mines come from, whether it's been dug yet, and DIG or FLAG. 86 bytes, written on every change and erased when the board is settled. |
| `minesweeper-history` | Save | Wins, losses and the last settled board's cells, for the menu. 84 bytes. |

**Copied code:** CrossPlay's `src/apps_local/minesweeper/MinesweeperCore.h` is `minesweeper_rules.h`, unchanged. `MinesweeperFlow.h` is `minesweeper_flow.h`, and `host-tests/minesweeper/test_minesweeper.cpp` is `tests/minesweeper/test_minesweeper.cpp`, both changed only in their includes. The screens and game are ported from `MinesweeperScreens.cpp` and `MinesweeperActivity.cpp`, redrawn on `core/` and `ui/`. All of it matched CrossPlay commit `0ce8d263` when last compared, and it's under the MIT licence, in `LICENSE-CrossPlay.txt`.

**Differences from CrossPlay:**

- `BACK` is a button in the header, because PocketPlay has no edge swipe. It leaves how to play, the board and the result for the menu, and leaves the game from the menu.
- **The board in play is saved**, on every change, packed byte by byte, so it survives sleep and leaving. The menu then offers `RESUME` and `NEW GAME`. CrossPlay keeps the board only while the game is open, and `PLAY` always deals a new one. A new game started over one in play isn't counted as a loss, as CrossPlay counts none.
- **The history is packed byte by byte** and refused if it doesn't check out, where CrossPlay writes a line of numbers.
- **Holding** uses PocketPlay's shared hold (`Game::usesHold()`). It fires 400 ms after the finger goes down, as CrossPlay's does, but only while the finger stays within the SDK's tap slop. CrossPlay restarts the hold on whichever cell the finger slides to. A hold always swallows its lift, as in CrossPlay.
- The hold border isn't drawn on a settled board, where a hold can't do anything.
- **Boards** are seeded from the chip's hardware random numbers, where CrossPlay uses the time since boot.
- The board starts below the warning line, at y 111, where CrossPlay's starts at 98, because PocketPlay's band is 66 tall and the line under it is kept for `NO CARD` and `SAVE FAILED`.
- The lessons are set in Size20 on every page. The longer ones take four lines, so the diagram starts below the text instead of 160 px down.
- The menu's rows are PocketPlay's, 62 tall with a 4 px gap, and the first is drawn selected, as CrossPlay draws it.
- A tap that changes nothing draws nothing: a flagged cell in DIG, an open cell that can't chord, any cell of a settled board, a hold on an open cell, and anywhere off the board and its buttons.
- Opening a board, `RESUME` included, is a full refresh, as starting a game is everywhere in PocketPlay, and so is the move that settles it.
- There are no button hints.

**Icons:** `minesweeper_icons.h` holds CrossPlay's home-page icon, from the Lucide `bomb` icon, and its flag, from the Lucide `flag` icon (both ISC licence). The flag is scaled to two thirds of its box, as CrossPlay draws it.

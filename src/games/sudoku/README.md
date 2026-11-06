# Sudoku

Classic 9x9, made on the device and graded by the hardest technique it needs: EASY needs only singles, MEDIUM locked candidates, HARD pairs, and EXPERT triples, X-wings, XY-wings and swordfish. Every puzzle has exactly one answer.

- **Board:** pick a digit on the pad, or tap a clue to pick up its digit, and every copy on the board gets corner marks. Tap an empty cell to write the armed digit, and tap your own digit again to clear it. Hold a cell to pencil the armed digit in, and hold again to rub it out; the cell darkens while the hold builds. A digit that clashes with one it can see turns black. `UNDO` takes back up to 24 changes, and `HINT` names a cell you can solve and the rule that solves it, or the first wrong digit. Once the grid is right the capsule says `SOLVED` and opens the result.
- **Result:** the time, your best at that level, the clue count and the hardest technique, then `ANOTHER` or `BACK TO THE GRID`. A solve that used a hint doesn't set a best time.
- **Menu:** where the puzzle stands, your solves and best time, the board in miniature, and `NEW PUZZLE` or `RESUME`. `DIFFICULTY` sets the level of the next puzzle.

**Saves:** both are in `/.pocketplay/`.

| Name | Kind | Holds |
|---|---|---|
| `sudoku` | Save | The puzzle in play: clues, your digits, pencil marks, the undo history, the armed digit, the hint, the time and the menu's level. The answer isn't saved: it's solved again from the clues on load. 353 bytes, written on every change. |
| `sudoku-record` | Save | Solves and best unaided times at each level, and hints taken. 26 bytes. |

**Copied code:** CrossPlay's `src/apps_local/sudoku/` files, each changed only in its includes: `SudokuCore.h/.cpp` is `sudoku_rules.h/.cpp`, `SudokuGame.h` is `sudoku_game.h`, and `SudokuFlow.h` is `sudoku_flow.h`. `host-tests/sudoku/test_sudoku.cpp` is `tests/sudoku/test_sudoku.cpp`. The screens and game are ported from `SudokuScreens.cpp` and `SudokuActivity.cpp`, redrawn on `core/` and `ui/`. All of it matched CrossPlay commit `0ce8d263` when last compared, and it's under the MIT licence, in `LICENSE-CrossPlay.txt`.

**Differences from CrossPlay:**

- `BACK` is a button in the header, because PocketPlay has no edge swipe. It leaves how to play and the board for the menu, the result for the board, and the game from the menu. Leaving the board while a puzzle is being made stops making it.
- **Saving:** the game is saved on every change, packed byte by byte, where CrossPlay writes a line of text when the board is left or a puzzle starts or ends. The record is a save of its own.
- **Undo survives sleep**, because the history is saved with the board. CrossPlay starts each visit with no undo.
- **The clock** counts while the board is open, as CrossPlay's does, but it's saved with each move, so the minutes between the last move and a sleep aren't counted.
- **The pad's keys are 64 px**, not 69, and the rail rows beside them 60 with 6 between. PocketPlay's grid starts under the warning line, 13 px lower than CrossPlay's, and with 69 px keys the pad would meet it.
- **Holding** uses PocketPlay's shared hold (`Game::usesHold()`), which fires 400 ms after the finger goes down while it stays within the SDK's tap slop. CrossPlay restarts the hold on whichever cell the finger slides to.
- A tap that changes nothing draws nothing: a clue whose digit is already armed, the pad key already armed, `UNDO` with nothing to undo, `HINT` saying the same thing again (`NOTHING YET`), the capsule before the grid is solved, and anywhere off the controls.
- While a puzzle is being made the grid is drawn empty, where CrossPlay draws the old board under `MAKING ONE`.
- Opening a board, `RESUME` included, is a full refresh, as starting a game is everywhere in PocketPlay, and so is the move that solves it. The board that arrives after `MAKING ONE` is a fast one.
- **Puzzles** are seeded from the chip's hardware random numbers, where CrossPlay uses the time since boot.
- The menu's rows are PocketPlay's, 62 tall with a 4 px gap. `DIFFICULTY` draws selected after it's tapped, as CrossPlay's list does.
- There are no button hints.

**Kept from CrossPlay as it is:** a solved puzzle stays saved, so the menu can draw it and the result can return to it. It's never offered as `RESUME`: the menu says `LAST ONE SOLVED` and `NEW PUZZLE`.

**Icon:** `sudoku_icon.h` is CrossPlay's home-page icon for this game, drawn from the Lucide `hash` icon (ISC licence).

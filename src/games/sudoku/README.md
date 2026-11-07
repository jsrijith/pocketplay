# Sudoku

The classic 9x9 number puzzle. Puzzles are made on the device at four levels, and every one has exactly one answer.

## Rules

- Fill every empty square with a digit from 1 to 9.
- Each row, each column and each 3x3 box must hold every digit from 1 to 9 exactly once.
- The digits already printed on the board (the clues) can't be changed.

**Levels:** each puzzle is graded by the hardest technique needed to solve it.

| Level | Hardest technique needed |
|---|---|
| EASY | Singles: a square with only one possible digit, or a digit with only one possible square |
| MEDIUM | Locked candidates |
| HARD | Pairs |
| EXPERT | Triples, X-wings, XY-wings and swordfish |

## Getting around

- **Menu:** where the current puzzle stands, your solves and best time, the board in miniature, and `NEW PUZZLE` or `RESUME`. `DIFFICULTY` sets the level of the next puzzle. Making a puzzle takes a moment; the board says `MAKING ONE` until it's ready.
- **Board:**
  - Pick a digit on the pad, or tap a clue to pick up its digit. Every copy of that digit on the board gets corner marks.
  - Tap an empty square to write the picked digit. Tap your own digit again to clear it.
  - Hold a square to pencil the picked digit in as a note, and hold again to rub it out. The square darkens while the hold builds.
  - A digit that clashes with the same digit in its row, column or box turns black.
  - `UNDO` takes back up to 24 changes.
  - `HINT` names a square you can solve and the technique that solves it, or points to the first wrong digit.
  - Once the grid is right, the capsule says `SOLVED`. Tap it to see the result.
- **Result:** your time, your best at that level, the clue count and the hardest technique, then `ANOTHER` or `BACK TO THE GRID`. A solve that used a hint doesn't set a best time.
- **How to play:** a short guide to the rules and controls.
- `BACK` in the header goes from the board or how to play to the menu, from the result to the board, and from the menu to the home page.
- The clock runs only while the board is open.

## Saves

Both in `/.pocketplay/`:

| Name | Holds |
|---|---|
| `sudoku` | The puzzle in play: clues, your digits, pencil marks, undo history, the picked digit, the hint, the time and the chosen level. Written on every change. A solved puzzle stays saved so the menu can show it, but it isn't offered to resume. |
| `sudoku-record` | Solves, best unaided times at each level, and hints taken. |

## Credits

Copied from CrossPlay and modified for PocketPlay, under the MIT licence (`LICENSE-CrossPlay.txt`). The icon is from Lucide (ISC licence).

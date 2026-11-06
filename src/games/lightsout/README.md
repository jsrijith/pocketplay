# Lights Out

A 5x5 board of lights. Tapping a square switches it and its four neighbours. Switch them all off.

**How it plays:**

- Every puzzle can be solved. It's made by pressing random squares on a dark board, and a solver finds the fewest presses that clear it, shown as the **target**: `MOVES 3   TARGET 6`.
- `HINT` frames one square of a fewest-press solution from where you are. `UNDO` takes back the last press and its move.
- The pill beside the record steps through `EASY`, `NORMAL` and `HARD`, which have targets of at least 3, 6 and 10. The level applies from the next `NEW`.
- When the last light goes off, the status turns solid, with a full refresh: `SOLVED IN 6 - FEWEST!`, or `SOLVED IN 9 - BEST IS 6`. `UNDO` and `HINT` dim, and `NEW` makes the next puzzle.
- `BACK` keeps the puzzle for later.

**Saves:**

- `lightsout`, version 1: the board, moves, target and up to 255 presses of undo history (8 to 263 bytes). It's written on every press and undo, and erased when the puzzle is solved. A save that doesn't load gives a fresh puzzle.
- `lightsout-stats`, version 1: the number of puzzles solved and the chosen level (5 bytes). It outlasts the puzzle's save.

**Translated code:** `lightsout_rules.h` is a C++ translation of the rules, solver and puzzle maker in touch-licorice-apps' `apps/lightsout/index.lua` (commit `3d7e622`) by Omer Faruk Inel, under the MIT licence in `LICENSE-licorice.txt`. The solver is the same: Gaussian elimination over GF(2), then each of the four combinations of the two free columns. `tests/lightsout/test_lightsout.cpp` checks what the source's `test_lightsout.py` checks, and also compares the solver with a search of all 2^25 press sets.

**Icon:** `lightsout_icon.h`, a 3x3 corner of the board with a cross of lights on, redrawn at 32 x 32 after the source's `icon.png`.

**Differences from the source:**

- The rules are translated from Lua to C++, and the screens are redrawn in PocketPlay's style: a black band with `BACK`, pills, and Jersey 25 in capitals.
- A win shows in a solid status capsule with a full refresh, instead of a box with `New` and `Close`. The board stays on screen.
- `UNDO` dims after a win. In the source, Undo after a win reopens the puzzle, and solving it again counts twice.
- The source's "Nothing to undo." message is gone: `UNDO` dims instead, and a tap on it does nothing.
- The level button sits beside the record, not in the bottom row, because `NORMAL` doesn't fit a quarter-width pill at Size14.
- Undo history is capped at 255 presses, and older ones drop off. The source keeps them all.
- Saves are packed byte by byte, versioned and checked, instead of `key=value` text. The record and level have their own file, so erasing a solved puzzle keeps them.
- Puzzles use xorshift32 seeded from the chip's hardware random numbers, instead of Lua's `math.random` seeded from the clock.
- PocketPlay draws nothing on a tap that changes nothing, such as a dimmed button, a tap on a solved board, or `HINT` again on the same square.

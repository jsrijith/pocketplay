# Games

Each game lives in its own folder and owns everything inside it.

```
games/
  game.h           what every game must provide
  game_list.inc    the list of games: one line each
  games.h/.cpp     looks games up from the list; no game-specific code
  tictactoe/       the example to copy
  wordle/          a larger example: several views, downloads and big saves
  dungeon/         a port, with a layout struct for taps
  connections/     a port with a download parsed as it arrives, and a layout tested on every board
  solitaire/       a port drawn in landscape
  minesweeper/     a port that acts on a held finger (Game::usesHold)
  sudoku/          a port that makes its puzzles between taps (Game::update)
  knucklebones/    a port with a computer opponent that plays between taps
  connectfour/     the same, with an opponent that searches seven moves ahead
  DESIGN.md        how a game's screens look
```

Read `DESIGN.md` before drawing anything, and use `tools/preview/run.sh` to see the screens on the PC.

## Adding a game

1. Create `src/games/<name>/`, with the folder name in lowercase and no spaces.
2. Implement `Game` from `games/game.h`, and define a function that returns it, such as `Game& ticTacToe()`.
3. Add one line to `game_list.inc`: `GAME(ticTacToe)`.

PlatformIO compiles every file under `src/`, so the build needs no changes. The game then appears on the home page and reopens after waking.

## What a game's folder holds

| File | Purpose | May use |
|---|---|---|
| `<name>.h`, `<name>.cpp` | The `Game`: screen and taps | `core/`, `ui/`, `games/game.h` |
| `<name>_rules.h` | The rules | Only the C++ standard library |
| `<name>_save.h`, `<name>_save.cpp` | The game state as bytes | Its own rules |
| `README.md` | How it plays, and what it saves | |
| `LICENSE-*.txt` | The licence of any code copied in, kept beside that code | |

Small games don't need every file, but keep rules and save packing free of screen and card code, so they can be tested off the device.

## The boundaries

- **Don't include another game's files.** If two games need the same code, move it to `ui/` or `core/` in a change of its own.
- **Keep everything inside the folder private.** Use a namespace named after the game for the rules and save code, and an unnamed namespace in `<name>.cpp`.
- **Keep game code out of the shared files.** A game shouldn't need changes to `shell/`, `ui/` or `core/`. If it does, make that change on its own first, because that's the part that can conflict with other branches.
- **Name save files after the folder:** `tictactoe`, or `tictactoe-stats` for a second one. `session`, `settings`, `networks` and `home` are already taken, as are each game's own (`tictactoe`, `wordle*`, `dungeon`, `connections*`, `solitaire*`, `minesweeper*`, `sudoku*`, `knucklebones*`, `connectfour*`, `suguru`, `sokoban*`, `queens*`, `symboldice`, `pegsolitaire`, `textadv*`, `wordladder`, `diceroller`, `nonograms*`, `kakuro`, `bingocaller`, `islandmaps`, `thinkalike`, `authoritytracker`), and all saves share `/.pocketplay/`.

The save rules are in `CLAUDE.md`, under Storage.

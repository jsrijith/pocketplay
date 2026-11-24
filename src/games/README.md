# Games

Each game lives in its own folder and owns everything inside it.

```
games/
  game.h           what every game must provide
  game_list.inc    the list of games: one line each
  games.h/.cpp     looks games up from the list; no game-specific code
  DESIGN.md        how a game's screens look
  <name>/          one folder per game, listed below
```

## The games

Each folder's `README.md` is for players. "From" says where a port came from; its licence sits beside the copied code.

| Folder | Game | From | Worth reading for |
|---|---|---|---|
| [`authoritytracker/`](authoritytracker/) | Authority Tracker | Original | The smallest original game: two scores and their buttons |
| [`bingocaller/`](bingocaller/) | Bingo Caller | Original | A random draw with no repeats, and a board of what's been called |
| [`chesspuzzles/`](chesspuzzles/) | Chess Puzzles | touch-licorice-apps | Built-in puzzle banks and a chess board you play a line out on |
| [`connectfour/`](connectfour/) | Connect Four | CrossPlay | A computer opponent that searches seven moves ahead between taps |
| [`connections/`](connections/) | Connections | CrossPlay | A download parsed as it arrives, and a layout tested on every board |
| [`crowncrest/`](crowncrest/) | Crown & Crest | Original | Fair dice: rejection sampling, checked over 600,000 rolls in the tests |
| [`diceroller/`](diceroller/) | Dice Roller | Original | One button and one result: a small screen to copy |
| [`dungeon/`](dungeon/) | D&Diagrams | CrossPlay | The worked example of a port, with a layout struct for taps |
| [`gridgambit/`](gridgambit/) | Grid Gambit | Original | Puzzles made between taps (`Game::update`), graded by a solver |
| [`islandmaps/`](islandmaps/) | Island Maps | Original | Up and Down stepping through a built-in list |
| [`karubatiles/`](karubatiles/) | Karuba Tiles | Original | A tile drawn pixel by pixel from a table of its paths |
| [`kakuro/`](kakuro/) | Kakuro | touch-licorice-apps | Digit keys for the selected cell, and the ways to make each run |
| [`knucklebones/`](knucklebones/) | Knucklebones | CrossPlay | A computer opponent that plays between taps |
| [`lastpiece/`](lastpiece/) | Last Piece | Original | Levels the player copies onto the card, checked as they're read |
| [`lightsout/`](lightsout/) | Lights Out | touch-licorice-apps | A small board game with hint and undo |
| [`minesweeper/`](minesweeper/) | Minesweeper | CrossPlay | Acting on a held finger (`Game::usesHold`) |
| [`nonogram/`](nonogram/) | Nonograms | touch-licorice-apps | Half-done puzzles kept in a blob; saves as `nonograms` |
| [`oneclue/`](oneclue/) | One Clue | Original | A deck of cards dealt without repeats, for a party game |
| [`pegsolitaire/`](pegsolitaire/) | Peg Solitaire | touch-licorice-apps | Two boards, undo and a best score per board |
| [`polyominopanic/`](polyominopanic/) | Polyomino Panic | Original | Drawing shapes from a deck for a game played on paper |
| [`queens/`](queens/) | Queens | touch-licorice-apps | Puzzle banks in six sizes, and hints |
| [`sokoban/`](sokoban/) | Sokoban | touch-licorice-apps | Swipes (`Game::onSwipe`), level packs and a best per level |
| [`solitaire/`](solitaire/) | Solitaire | CrossPlay | A game drawn in landscape |
| [`sudoku/`](sudoku/) | Sudoku | CrossPlay | Puzzles made between taps (`Game::update`), with pencil marks |
| [`suguru/`](suguru/) | Suguru | touch-licorice-apps | Outlined regions on a grid, in four banks |
| [`symboldice/`](symboldice/) | Symbol Dice | Original | Dice of symbols and a turn counter for a roll-and-write game |
| [`textadv/`](textadv/) | Text Adventures | touch-licorice-apps | Story files from the card, and the Z-machine run between taps |
| [`thinkalike/`](thinkalike/) | Think Alike | Original | A text file the player edits on the card, reshuffled when it changes |
| [`tictactoe/`](tictactoe/) | Tic Tac Toe | CrossPlay | The smallest game: the example to copy |
| [`wordladder/`](wordladder/) | Word Ladder | touch-licorice-apps | A word list in a blob and a daily puzzle |
| [`wordle/`](wordle/) | Wordle | CrossPlay | A larger example: several views, downloads and big saves |
| [`yachtdice/`](yachtdice/) | Yacht Dice | CrossPlay | A turn of several steps, and a computer opponent paced so its rolls can be seen |

Read `DESIGN.md` before drawing anything, and use `tools/preview/run.sh` to see the screens on the PC.

## Adding a game

1. Create `src/games/<name>/`, with the folder name in lowercase and no spaces.
2. Implement `Game` from `games/game.h`, and define a function that returns it, such as `Game& ticTacToe()`.
3. Add one line to `game_list.inc`: `GAME(ticTacToe)`, and a row to the table above.
4. Add a line to `tools/preview/screenshots.sh` with the taps that reach a screen of play, run `tools/preview/screenshots.sh <name>`, and add a `## Screenshot` section to the game's `README.md` like the other games'.
5. In the root `README.md`, add a row to the table under "Games included" that fits the game (word, logic, chess, card and board, or tabletop companion), with its name linked to its `README.md`, and raise the count in the opening ("a home page of N games") to the number of lines in `game_list.inc`. If the game was ported, raise the count of games from its source in the same paragraph.

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
- **Name save files after the folder:** `tictactoe`, or `tictactoe-stats` for a second one. All saves share `/.pocketplay/`, so treat every name that starts with an existing folder's name as taken, along with `session`, `settings`, `networks` and `home`, which the shell and Settings use. One game breaks the rule: `nonogram/` saves as `nonograms` and `nonograms-grids`, so those are taken too. Don't rename them, or existing saves stop loading.

The save rules are in `CLAUDE.md`, under Storage.

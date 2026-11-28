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

## Making a game multiplayer

Two devices can play a game over the radio (`core/link/`, from CrossPlay). Yacht Dice is the worked example: its `PLAY NEARBY` is in `yachtdice.cpp`, and `tests/yachtdice/test_yachtdice_link.cpp` plays whole games between two devices on the PC.

1. **Choose the shared state:** usually the whole game, as a struct that is trivially copyable and at most 192 bytes. For a CrossPlay port, use CrossPlay's struct unchanged and its `GameId`, and `static_assert` the struct's size, so the game also plays CrossPlay devices. Otherwise take a `GameId` in `0xE0xx`, add it to `kAllGameIds` in `core/link/play.h`, and raise it whenever the struct's layout changes.
2. **Hold a `linkplay::Play<State>` and then a `linkplay::Match`**, in that order, and implement `linkplay::MatchHost`: `onMatchStart(goesFirst)` sets up a fresh game; `takeOpponentState()` takes the state with `play.takeOpponent()` into a scratch copy and keeps it only if the rules could have made it; `matchGameOver()`; `onMatchEnded()` records the result and shows the final board; `onLinkEnded()` goes back to the menu.
3. **Keep the match apart from the solo game**, in its own state, so a match never overwrites a game that's saved to resume. A match isn't saved.
4. **Wire it in:** a `PLAY NEARBY` row calls `match.enter(gameId, linkplay::deviceName())` and says so if it returns false. `update()` returns `match.update(millis())` while `match.active()`. `onTap()` and `draw()` hand over to `ui/link/link_screens.h` while `match.ownsScreen()`, and `draw()` calls `match.notePainted(millis())` after the game's own screen. After your move, `play.play(state)`, and only while `match.yourTurn()`. `BACK` calls `match.leave()`. `busy()` returns `match.keepAwake()`.
5. **Test it** over `tests/link/FakeLink.h`, with a lossy link as well as a clean one, and check both devices end on the same state.

Only one device's random numbers may count: the side that moves first deals, and its state carries the seed.

`goesFirst` is only right for the first game. The link doesn't reset the turn for another one: whoever didn't send the last move goes next, and `onMatchStart()` gets the first game's coin toss again. A game whose matches end after an even number of moves, as Yacht Dice's do, never notices. One that ends on the winner's move, as Tic Tac Toe does, has to work out who moves first itself (`tictactoe_flow.h`'s `nextMark()`).

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

# Changelog

What's new in each release, written for players. Each release's notes on GitHub take their "What's new" from its section here.

Before a release, add what changed under `## Unreleased` at the top, and commit it. `scripts/release.sh` renames that heading to the version it makes. A release without a section here falls back to the list of commits.

## Unreleased

- **Dice Roller:** a count of your rolls under the total, and a `RESET` button that clears the dice and starts the count again from 0.
- **Crown & Crest:** a count of the rolls under the dice, and a `RESET` button that starts a new game. Tap it twice to confirm: the first tap turns it into `SURE?`.

## 0.5.0

- **D&Diagrams:** the chosen dungeon's name now shows at the top of the map, right above the rooms. Step through the dungeons with the Up and Down buttons: Up goes left, Down goes right, and both wrap around from 64 to 1.

## 0.4.0

- **Last Piece:** a new game of numbered chess challenges on a 4x4 board, where every move takes a piece and one piece left wins. The challenges and their solutions are text files you copy onto the card, in four difficulties. Step through the challenges with Up and Down, and tap RESET to start one again.

## 0.3.0

- **Apps:** a new grid icon beside the Settings cog opens a page of apps: tools rather than games.
- **Calculator:** the first app, a pocket calculator with + − × ÷, percent and backspace. Its number is kept when the device sleeps.

## 0.2.0

- **Island Maps:** a new `FIRST MAP` button, beside `RANDOM`, jumps back to Forbidden Island, so you can step through the islands in order from there with Down.

## 0.1.0

The first release: 29 games, all played by touch and designed for e-ink:

Wordle, Tic Tac Toe, D&Diagrams, Connections, Solitaire, Minesweeper, Sudoku, Knucklebones, Connect Four, Lights Out, One Clue, Suguru, Sokoban, Queens, Symbol Dice, Peg Solitaire, Text Adventures, Word Ladder, Dice Roller, Nonograms, Kakuro, Bingo Caller, Island Maps, Think Alike, Authority Tracker, Grid Gambit, Chess Puzzles, Polyomino Panic and Crown & Crest.

Every game saves as you play, so sleep or a flat battery brings back the same position. Wi-Fi is used only for downloading puzzles, and the clock is set from the network.

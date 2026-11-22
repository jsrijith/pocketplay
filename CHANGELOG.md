# Changelog

What's new in each release, written for players. Each release's notes on GitHub take their "What's new" from its section here.

Before a release, add what changed under `## Unreleased` at the top, and commit it. `scripts/release.sh` renames that heading to the version it makes. A release without a section here falls back to the list of commits.

## 0.9.0

- **PocketBGG:** a `RANDOM` button beside `FILTER`, which opens one of the games the filter shows, picked at random. Set the filter to owned games for 4 players, say, and let it choose tonight's game. Tapping it again never picks the same game twice in a row.
- **PocketBGG:** Up and Down wrap around. On a game's details, Down on the last game goes to the first and Up on the first to the last, and on the list, Down on the last page goes back to page 1.

## 0.8.0

- **Karuba Tiles:** a new companion for the Karuba board game. `DRAW`, or the Down button, draws one of the 36 jungle tiles at random with no repeats, shows its paths, its number in large type and its crystal or gold, lists the three drawn before it, and keeps a board of every tile that's out.
- **Crown & Crest:** a crown for its home-page icon, so it no longer looks like Authority Tracker's shield.
- **Sleep screen:** no version and build line along the bottom, just the logo and `PRESS OK TO WAKE`. The boot screen and Settings' About page still show the version.

## 0.7.0

- **PocketBGG**, a new app on the APPS page: your BoardGameGeek collection on the device. Put your BGG username and your own API token in `/pocketbgg/config.txt` on the card, and `SYNC` downloads your collection, each game's last-played date from your logged plays, and each game's cover, dithered to black and white. It all stays on the card, so the list works offline. Filter by status, played or not and player count, sort by name or last played, and step through games' details with Up and Down. A first sync takes about 7 minutes, and later ones about 4.
- **All apps and games:** more room on the stack for the main loop, which a PNG cover needed. Nothing changes that you'd see.

## 0.6.0

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

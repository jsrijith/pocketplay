# Tic Tac Toe

Two players on one device. Every move is saved to the SD card, so sleep, a crash or a flat battery returns you to the same board. `BACK` keeps the game for later, and finishing it or tapping `NEW GAME` clears the save.

**Saves:** `tictactoe`, holding the board and whose turn it is. It's erased when the game ends.

**Copied code:** `tictactoe_rules.h` is CrossPlay's `src/apps_local/tictactoe/TicTacToeCore.h`, unchanged, and matched CrossPlay commit `fbeac762` when last compared. It's under the MIT licence, in `LICENSE-CrossPlay.txt`.

**Icon:** `tictactoe_icon.h` is CrossPlay's home-page icon for this game, drawn from the Lucide `circle-x` icon (ISC licence).

**Differences from CrossPlay:** the header carries a `BACK` button, because PocketPlay has no edge swipe. The rules are worded more briefly.

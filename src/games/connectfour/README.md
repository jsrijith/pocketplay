# Connect Four

Seven columns, six rows, against the computer. Tap a column and your disc falls to the lowest free place in it. Four of yours in a line, across, up or on either diagonal, wins, and a full board with no line is a draw. You play the outlined discs and always go first. The computer plays the solid ones, searching seven moves ahead.

- **Board:** the slots across the top of the frame hold the disc you're about to drop, one in each column with room, and go grey while it's the computer's turn or once a column is full. The last disc is marked with a dot. Under the board, the discs each side has left. The computer drops its disc straight after yours, and the result follows the disc that settles the game.
- **Result:** the final board, with the winning four struck through, then `PLAY AGAIN` or `DONE`.
- **Menu:** games played and won, the last game's final board with its four marked, and `PLAY`, or `RESUME` and `NEW GAME` while a game is unfinished.

**Saves:** both are in `/.pocketplay/`.

| Name | Kind | Holds |
|---|---|---|
| `connectfour` | Save | The game in play: the board, where the last disc landed, and whose turn it is. 45 bytes, written after every disc and erased when the game ends. |
| `connectfour-record` | Save | Wins, losses and draws, and the last game's final board, how it ended and its winning four. 54 bytes. |

**Copied code:** CrossPlay's `src/apps_local/connectfour/` files, each changed only in its includes: `ConnectFourCore.h` is `connectfour_rules.h`, `ConnectFourBrain.h` (the opponent) is `connectfour_brain.h`, and `ConnectFourFlow.h` is `connectfour_flow.h`. `host-tests/connectfour/test_connectfour.cpp` is `tests/connectfour/test_connectfour.cpp`. The screens and game are ported from `ConnectFourScreens.cpp` and `ConnectFourActivity.cpp`, redrawn on `core/` and `ui/`. All of it matched CrossPlay commit `fbeac762` when last compared, and it's under the MIT licence, in `LICENSE-CrossPlay.txt`.

CrossPlay's test plays many whole games with the seven-move search, so it's the slowest suite in `scripts/check.sh`: about 40 seconds under the sanitizers.

**Differences from CrossPlay:**

- `BACK` is a button in the header, because PocketPlay has no edge swipe. It leaves how to play, the board and the result for the menu, and the game from the menu.
- **No `PLAY NEARBY`:** PocketPlay has no two-device play yet, so there's only the built-in opponent.
- **Saving:** the game is saved after every disc, packed byte by byte, so it survives sleep and is offered as `RESUME`. CrossPlay keeps no game, and leaving one loses it.
- **The record** is packed byte by byte too, where CrossPlay writes a line of text. It's written the moment a disc settles the game, so leaving before the result still counts it.
- **The board** has CrossPlay's sizes, but starts under PocketPlay's warning line, 10 px lower.
- **How to play** turns its pages with `NEXT` only. CrossPlay also steps them with the Up and Down buttons, and PocketPlay's games use touch alone.
- A tap that changes nothing draws nothing: a full column, any tap on the computer's turn, and anywhere off the board and controls.
- Starting or resuming a game is a full refresh, as starting a game is everywhere in PocketPlay, and each disc is a fast one. The result arrives between taps, after the last disc is drawn, so it's a fast refresh too.
- The menu's rows are PocketPlay's, 62 tall with a 4 px gap, and the first reads selected, as CrossPlay's does.
- There are no button hints.

**Kept from CrossPlay as it is:**

- The computer thinks in the main loop, so taps aren't read until it has moved.
- On the menu's small board, the winning four are marked with black squares, which can't be seen on the computer's solid discs, so a lost game shows no mark there. The result screen strikes the line through in white on solid discs.

**Icon:** `connectfour_icon.h` is CrossPlay's home-page icon for this game: four discs on a diagonal.

# Knucklebones

A dice game against the computer, on two 3x3 grids facing each other. Each turn a die is rolled, and you drop it into one of your three columns. A column scores each value as value × count × count, so three 4s are worth 36, not 12, and dice that multiply are drawn inverted. Placing a value knocks every copy of it out of the opponent's facing column. The match ends the moment either grid fills, and the higher total wins.

- **Board:** the opponent's grid on a grey ground at the top, yours below, and the die between the two totals. Brackets mark the columns that still have room while it's your roll. The opponent places its die straight after yours, one placement per redraw, and the result follows the last die.
- **Result:** who won and the score, then `PLAY AGAIN` or `DONE`.
- **Menu:** matches played and won, the grids the last match ended on with the win, loss and draw tally between them, and `PLAY`, or `RESUME` and `NEW GAME` while a match is unfinished.

**Saves:** both are in `/.pocketplay/`.

| Name | Kind | Holds |
|---|---|---|
| `knucklebones` | Save | The match in play: both grids, whose turn it is, the die waiting, and the random state the next dice come from. 24 bytes, written after every placement and erased when the match ends. |
| `knucklebones-record` | Save | Wins, losses and draws, and the two grids the last match ended on. 25 bytes. |

**Copied code:** CrossPlay's `src/apps_local/knucklebones/` files, each changed only in its includes: `KnucklebonesCore.h` is `knucklebones_rules.h`, `KnucklebonesBrain.h` (the opponent) is `knucklebones_brain.h`, and `KnucklebonesFlow.h` is `knucklebones_flow.h`. `host-tests/knucklebones/test_knucklebones.cpp` is `tests/knucklebones/test_knucklebones.cpp`. The screens and game are ported from `KnucklebonesScreens.cpp` and `KnucklebonesActivity.cpp`, redrawn on `core/` and `ui/`. All of it matched CrossPlay commit `fbeac762` when last compared, and it's under the MIT licence, in `LICENSE-CrossPlay.txt`.

**Differences from CrossPlay:**

- `BACK` is a button in the header, because PocketPlay has no edge swipe. It leaves how to play, the board and the result for the menu, and the game from the menu.
- **No `PLAY NEARBY`:** PocketPlay has no two-device play yet, so there's only the built-in opponent.
- **Saving:** the match is saved after every placement, packed byte by byte, so it survives sleep and is offered as `RESUME`. CrossPlay keeps no match, and leaving one loses it.
- **The record** is packed byte by byte too, where CrossPlay writes a line of text. It's written the moment a placement ends the match, so leaving before the result still counts it.
- **The cells are 70 px**, not 73. PocketPlay's board starts under the warning line, 22 px lower than CrossPlay's, and with 73 px cells your column scores would meet the status capsule. The gaps are CrossPlay's.
- A tap that changes nothing draws nothing: a full column, the opponent's grid, the gaps between your columns, any tap on the opponent's turn, and anywhere off the controls.
- Starting or resuming a match is a full refresh, as starting a game is everywhere in PocketPlay, and each placement is a fast one. The result arrives between taps, after the last die is drawn, so it's a fast refresh too.
- **Matches** are seeded from the chip's hardware random numbers, where CrossPlay uses the time since boot.
- The menu's rows are PocketPlay's, 62 tall with a 4 px gap, and the first reads selected, as CrossPlay's does.
- There are no button hints.

**Icon:** `knucklebones_icon.h` is CrossPlay's home-page icon for this game, drawn from the Lucide `dice-5` icon (ISC licence).

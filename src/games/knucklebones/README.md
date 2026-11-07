# Knucklebones

A dice game against the computer, on two 3x3 grids facing each other. Fill your grid with the dice that score most, and knock the computer's dice out.

## Rules

- Each turn a die is rolled, and you drop it into one of your three columns that still has room.
- A column scores each value as value × count × count, so three 4s are worth 36, not 12. Dice that multiply are drawn inverted.
- Placing a value knocks every die of that value out of the computer's facing column.
- The match ends the moment either grid fills, and the higher total wins.

## Getting around

- **Menu:** matches played and won, the grids the last match ended on with your win, loss and draw tally, and `PLAY`. While a match is unfinished, it shows `RESUME` and `NEW GAME` instead.
- **Board:** the computer's grid is at the top on a grey ground, and yours is below, with the die and both totals between them. On your roll, brackets mark the columns you can drop into. The computer places its die straight after yours.
- **Result:** who won and the score, then `PLAY AGAIN` or `DONE`.
- `BACK` in the header goes from the board, result or how to play to the menu, and from the menu to the home page.

## Saves

All in `/.pocketplay/`:

| Name | Holds |
|---|---|
| `knucklebones` | The match in play. Written after every placement, so sleep or a flat battery brings you back to it, and erased when the match ends. |
| `knucklebones-record` | Wins, losses and draws, and the grids the last match ended on. |

## Credits

Copied from CrossPlay and modified for PocketPlay, under the MIT licence (`LICENSE-CrossPlay.txt`). The icon is from Lucide (ISC licence).

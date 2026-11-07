# Solitaire

Klondike, the classic patience game. It's the one game played in landscape: from the home page, turn the device a quarter-turn anticlockwise, so the USB edge is at the top.

## Rules

- The table has seven columns, a stock (face-down pile) and a waste pile in the top corner, and four foundations.
- **Goal:** build all four foundations from ace up to king, one suit each.
- **Columns:** a card can go on a card one rank higher of the other colour, such as a red 6 on a black 7. You can move a face-up card together with every card on top of it. When a face-down card is uncovered, it turns face up.
- **Empty column:** only a king, or a run starting with a king, can go there.
- **Stock:** tap it to turn cards onto the waste, one or three at a time. Only the top waste card can be played. When the stock runs out, tap it (it shows a ring) to turn the waste back over and go through again.
- You win when all 52 cards are on the foundations.

## Getting around

- **Menu:** shows whether a deal is in play and how many moves deep it is, your record (dealt, cleared, wins in a row), and your last sixteen games as a hand of cards: face up for a win, grey for a deal left unfinished, an outline for a game not played yet.
  - `PICK IT UP` returns to the deal in play.
  - `DEAL` starts a new deal.
  - `TURNING ONE` / `TURNING THREE` sets how many cards the stock turns, for the next deal.
- **Board:**
  - Tap a card to pick it up, with any cards on top of it, then tap where it should go.
  - A card that can go straight to a foundation goes there on the first tap.
  - Tapping somewhere the card can't go picks up what you tapped instead, and tapping its own pile puts it back down.
  - `UNDO` takes back up to 96 moves, and `NEW` deals again.
  - Once every card is face up, `FINISH` plays the rest for you.
- **Win:** the move count and your record, then `DEAL AGAIN` or `BACK TO THE TABLE`.
- `BACK` in the header goes from the board or win screen to the menu, and from the menu to the home page.
- The game always opens on the menu, so after waking, tap `PICK IT UP` to carry on. A card that was picked up when the device slept is put back down.

## Your record

Leaving a deal for a new one after making a move counts as a loss, so restarting bad deals can't build a streak.

## Saves

All in `/.pocketplay/`:

| Name | Holds |
|---|---|
| `solitaire` | The deal in play: every pile, the move count, the draw mode and the undo history. Written on every move and erased when the game is won. |
| `solitaire-results` | One entry per finished game (won, or left unfinished), up to the last 2,048. |

## Credits

Copied from CrossPlay and modified for PocketPlay, under the MIT licence (`LICENSE-CrossPlay.txt`). The icon is from Lucide (ISC licence).

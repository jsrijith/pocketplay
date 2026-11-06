# Solitaire

Klondike. Build the four foundations up from ace to king, one suit each, by moving cards down the seven columns in alternating colours. An empty column takes only a king. It's the one game drawn in landscape: turn the device a quarter-turn anticlockwise from the home page, so the USB edge is at the top.

- **Menu:** whether a deal is in play and how many moves deep, the record (dealt, cleared, wins in a row), and your last sixteen games as a hand of cards: face up for a win, grey for a deal left unfinished, an outline for one not played yet. `PICK IT UP` goes back to the deal in play, `DEAL` starts a new one, and `TURNING ONE` or `TURNING THREE` sets how many cards the stock turns, for the next deal.
- **Board:** tap the stock to turn cards onto the waste, or to turn the waste back over once the stock is empty (the ring). Tap a card to pick it up with the run on top of it, then tap where it goes. A card that can go straight to a foundation goes there on the first tap. Tapping somewhere it can't go picks up what you tapped instead, and tapping its own pile puts it down. `UNDO` takes back up to 96 moves, and `NEW` deals again. Once every card is face up, `FINISH` plays the rest.
- **Win:** a full refresh, the move count and the record, then `DEAL AGAIN` or `BACK TO THE TABLE`.
- **Record:** leaving a deal for a new one after making a move counts as a loss, so restarting bad deals can't build a streak.

**Saves:** both are in `/.pocketplay/`.

| Name | Kind | Holds |
|---|---|---|
| `solitaire` | Save | The deal in play: every pile, the move count, the draw mode, and the undo history. At most 265 bytes, written on every move and erased when the game is won. |
| `solitaire-results` | Blob | One byte per finished game, oldest first: won, or left unfinished. At most 2,048 games, then the oldest goes. |

**Copied code:** from CrossPlay's `src/apps_local/`, changed only in their includes unless noted:

- `solitaire/SolitaireCore.h/.cpp` is `solitaire_rules.h/.cpp`, with one addition marked `POCKETPLAY`: `undoDepth()`, `undoStep()` and `restoreUndo()`, so the undo history can be saved. Nothing else in the rules changed.
- `cards/Cards.h` is `cards.h`.
- `cards/CardSuits.h` is `card_suits.h`, its suit art. `freeink::Icon` is renamed `SuitIcon` (`suit_icon.h`), so it needs no SDK library.
- `host-tests/solitaire/test_solitaire.cpp` is `tests/solitaire/test_solitaire.cpp`.

`card_art.cpp` is `cards/CardArt.cpp`, and the screens and game are `SolitaireScreens.cpp` and `SolitaireActivity.cpp`, all redrawn on `core/` and `ui/` with CrossPlay's geometry. All of it matched CrossPlay commit `0ce8d263` when last compared, and it's under the MIT licence, in `LICENSE-CrossPlay.txt`. The suit marks are the card-suit glyphs of Noto Sans Symbols 2, under the SIL Open Font License, in `OFL-Noto.txt`.

**Differences from CrossPlay:**

- `BACK` is a button in the header, because PocketPlay has no edge swipe. It leaves the board and the win screen for the menu, and leaves the game from the menu.
- **Undo survives sleep.** The undo history is saved with the board, and is restored only if every step undoes legally from the board on the card. CrossPlay starts each visit with no undo.
- **Saving:** the board is saved on every move, packed byte by byte, where CrossPlay writes a copy of its board struct every ten moves and when the board is left. The save holds only the 52 cards rather than every pile's full capacity.
- **A save that doesn't check out is set aside** rather than played: a wrong size, a card out of range or face the wrong way for its pile, or a deck that isn't 52 distinct cards. CrossPlay checks the deck only.
- **Deals** are seeded from the chip's hardware random numbers, where CrossPlay uses the time since boot.
- `UNDO` and `NEW` are white pills in the band, the same size as `BACK`. An `UNDO` with nothing to take back is grey, as a dimmed pill is everywhere in PocketPlay, where CrossPlay draws it as an outline. The shell's warning (`NO CARD`, `SAVE FAILED`) sits in the band between the title and `UNDO`, because the cards start right below the band.
- `FINISH` is set in Size14, the cut PocketPlay's pills use, because at Size20 the word is 101 px, wider than the 82 px button.
- A tap that changes nothing draws nothing: a tap on an empty part of the table, the stock when it and the waste are both empty, `UNDO` with nothing to take back, a card that can't be picked up, and an empty column or foundation with nothing in hand or where the card in hand can't go.
- Opening a board, `PICK IT UP` included, is a full refresh, as starting a game is everywhere in PocketPlay. A win is too, as in CrossPlay.
- There are no button hints.

**Kept from CrossPlay as it is:**

- The game always opens on the menu, so a wake comes back to `PICK IT UP`, not to the board. A card picked up when it went to sleep is put down.
- The draw mode belongs to a deal. Changing it with no deal in play and then sleeping goes back to `TURNING ONE`.
- A covered card shows a 30 px strip, and the bottom few pixels of its rank fall under the next card, which can make a Q look like an O in a fanned column. The card's suit still shows beside it.
- `FINISH` sits in the gap after the waste, and a three-card fan reaches into it, so with three waste cards showing it covers the right edge of the top one, corner suit included. The rank still shows.

**Icon:** `solitaire_icon.h` is CrossPlay's home-page icon for this game, drawn from the Lucide `spade` icon (ISC licence).

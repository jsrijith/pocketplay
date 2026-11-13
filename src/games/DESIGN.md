# How a game looks

PocketPlay's design is built for a black-and-white e-ink panel held in one hand: big type, few refreshes, and black spent only where it never changes. It began from CrossPlay's Toybox and has gone its own way. Every screen is drawn with the calls in `src/ui/ui.h`, and `home_screen.cpp` and `games/dungeon/` are the examples to copy.

## The screen

Portrait, 480 x 800, from the top:

| y | What |
|---|---|
| 0–66 | The black band: `ui::drawBand(frame, title, rightLabel, true)`. `BACK` is on the left, because PocketPlay has no edge swipe. |
| 69–72 | The rule under the band, drawn by `drawBand`. |
| 76–105 | The warning line (`NO CARD`, `SAVE FAILED`). The shell draws it, so leave it empty. |
| 108 (`ui::CONTENT_TOP`) | Content starts. |
| 732–784 | The action row: pills 52 tall (`ui::PILL_HEIGHT`) above a 16 px bottom margin. |

The side margin is 16 px, and the gap between blocks is 12. `ui::MARGIN` (12) is the old header's margin, so take 16 for a game's content.

**Anchor layouts; don't float them.** Put the board under the header and the buttons at the bottom margin, so the screen reads as a page with a footer. Any spare space then becomes one deliberate gap.

## Landscape

A game whose `landscape()` returns true is drawn 800 x 480, with the USB edge at the top. Use it only when the game needs the width, as Solitaire's seven columns do.

| y | What |
|---|---|
| 0–56 | The band, `ui::drawBand()` as in portrait, but 56 tall (`ui::LANDSCAPE_BAND_HEIGHT`), with `BACK` centred in it. |
| 59–62 | The rule under the band. |
| 62 (`ui::LANDSCAPE_CONTENT_TOP`) | Content can start, after a gutter. |

There's no warning line under the band. The shell draws its warning in the band, between x 428 and 572 (`ui::LANDSCAPE_WARNING_LEFT`), so `drawBand` keeps the title left of that, and a game's own band buttons go to its right.

## Type

Jersey 25 only (`core/text.h`), in these roles:

| Cut | Used for |
|---|---|
| Size30 | Band titles, a board's numbers, status capsules, headlines |
| Size20 | List rows, body text, a game's labels |
| Size14 | Pill labels, rules text, a small right label such as a date |
| Size10 | Captions such as "TAP TO CHOOSE, THEN PLAY" |

- **Never shorten text.** A cut name shows the player a different word. Step down a cut until it fits: `drawBand` does this for titles. For anything wrapped, choose the cut with `wrappedHeight()`, which breaks lines the same way `drawWrapped()` does. Never use `fitText()` for something the player has to read in full.
- **Centre capitals with `drawTextInBox()`**, and left-align with `ui::drawTextLeft()`. Neither centres on the font's line box, which would make capitals hang low.
- **Grids of equals share one cut.** If tiles or keys sit side by side, size them all to the widest one.

## Black

**The black you can afford is inversely proportional to how often it changes.** A black header that never repaints costs nothing. The same amount of black on a board that repaints every move ghosts and slows the refresh.

| Element | Changes | Treatment |
|---|---|---|
| Header band | never | solid black, white type |
| A board's cells | every move | outlines, and black only for the player's own marks |
| A selection | every move | corner brackets or a 4 px frame, never a fill |
| A status capsule | every move | outlined; solid only at game over, with the full refresh |
| A spent or unavailable button | rarely | `ui::drawDimmedPill`: grey, never removed |

The only grey is dither: one dot in four (`x % 2 == 0 && y % 2 == 0`). Type can't be grey, so dim a control by greying its background.

## Controls

- **Buttons are pills.** Use `ui::drawPill(..., true)` for actions, and outlined for a lesser one such as `TUTORIAL`. Two actions split the bottom row with a 12 px gap. One main action takes the whole width.
- **Lists are rows.** `ui::drawRow`: 62 tall with a 4 px gap, a hairline outline, and a Size20 label.
- **A button is a region, not a screen.** Only the drawn shape answers. "Tap anywhere to continue" throws away a finished game the player was still reading.
- **Hit areas use the geometry that drew.** Have the draw code fill in a layout struct that the tap code reads, as `dungeonui::Layout` and `PickerLayout` do, so the two can't drift apart.
- **A hold is a second verb, for a board cell only.** A game that needs one (Minesweeper's flag) sets `usesHold()`, marks the cell under the finger from `onPress` so the hold can be seen building, and clears it in `onRelease`. Keep a button for the same action, because nothing on screen says a hold exists.
- **Touch points; buttons step.** A cell or card is chosen by touch. The Up and Down keys only step through pages. There's no on-screen cursor.
- **A control that can't act dims; it doesn't disappear.** That way the layout never jumps.
- **Size a control to its widest label, once.** That way it doesn't grow and shrink as its text changes.

## Strokes

`ui::HAIRLINE` 1 for cells and secondary outlines, `ui::RULE` 3 for dividers and outlined pills, `ui::FRAME` 4 for selections, and `ui::BOARD_FRAME` 9 around a board. Keep weights far enough apart to read as different.

## Refresh as punctuation

Use `Redraw` (fast) for a move, and `RedrawFull` (the blink) for starting a game, winning or losing. The blink is the payoff, so spend it only there. A tap that changes nothing returns `Nothing` and draws nothing.

## Ornament

**Decoration has to carry the player's own data, in the game's own material.** D&Diagrams' map is the player's cleared dungeons, and Wordle's chart is their guesses per win. Ask whether a screenshot of it would look the same on everyone's device. If it would, it's wallpaper: give it data or remove it.

## The front door

A game with a menu has a headline (the main action, and the biggest thing on the screen), the state, a rule, a one-line record, an ornament, then the lesser actions anchored at the bottom.

## The home-page icon

32 x 32, rows padded to whole bytes, MSB first, 0 = ink, returned by `icon()`. Draw it from a [Lucide](https://lucide.dev) icon (ISC licence), and name that icon in a comment and in the README.

## Checking it

Run `tools/preview/run.sh` and look at every screen before flashing. For a port, compare them with the source game's screenshots too.

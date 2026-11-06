---
name: new-game
description: Add a game to PocketPlay, either new or ported ("lifted and shifted") from CrossPlay in /home/srijith/mycrossplay. Use whenever the user asks to create, add, port, copy or bring over a game, or to restyle a game's screens.
---

# Adding a game to PocketPlay

The rules live in the repo, not here. Read these first, in full:

1. `CLAUDE.md`, especially **Games**, **Storage**, and **Copying a game from CrossPlay**.
2. `src/games/README.md`: the folder layout, the boundaries, and how to list a game.
3. `src/games/DESIGN.md`: how a screen is built, and which `ui::` calls to use.
4. `tools/preview/README.md`: how to see the screens on the PC.

`src/games/dungeon/` is the worked example of a port, and `src/games/tictactoe/` is the smallest game.

## Steps

1. **Understand the source.** For a port, read CrossPlay's `src/apps_local/<name>/`, its `host-tests/<name>/`, any `docs/apps/<name>.md`, and its screenshot `site/assets/shots/<name>.png`. For a new game, agree the rules with the user first.
2. **Ask before writing**, with AskUserQuestion, about anything that changes what gets built: the game's `name()` (it's saved in `session.sav`, so it's hard to change later), and any CrossPlay feature that needs something PocketPlay lacks, such as multiplayer, the network or a second font.
3. **List every difference from CrossPlay for the user, before building**, as CLAUDE.md requires. Include what PocketPlay does that CrossPlay doesn't: the `BACK` button, saving on every change, byte-by-byte packing, `Nothing` for taps that change nothing, and the refresh choices.
4. **Shared changes come first, in their own commit.** If the game needs something in `core/` or `ui/`, add and commit that separately, so game branches don't conflict over it.
5. **Copy the rules and test unchanged except for includes**, with `LICENSE-CrossPlay.txt` beside them. Add a save module and its test if the game saves. Add `tests/<name>/run.sh`, which takes its flags from `tests/flags.sh`.
6. **Write the screens and the `Game`** on `core/` and `ui/` only, following DESIGN.md. Add the icon, `README.md` (how it plays, its saves, the copied code, the differences), and one line in `game_list.inc`. Add a line to the root README's game list.
7. **Preview every screen.** Run `tools/preview/run.sh` and look at each PNG with the Read tool. Compare it with the CrossPlay screenshot. Check that each tap prints the `TapResult` you expect, and fix anything that's cut off, overlapping or misaligned before going further.
8. **Check and build:** run `scripts/check.sh` (only `CHECK-VERDICT: green` passes) and `./scripts/package.sh`.
9. **Hand over:** commit as described in the commit rules, show the user the preview images, give them the PowerShell flash commands from CLAUDE.md, and list what can only be checked on the device. Don't push unless they ask.

## Mistakes made before

- Drawing type a size too big: rows are Size20, band titles Size30, and pill labels Size14.
- Forgetting that the header holds `BACK`: a long title has less room and drops a cut.
- A shortened name, or `fitText()` with "..", where the text has to be read in full.
- `CLEAR` or a refused tap returning `Redraw`, so the screen blinks for nothing.
- Hit areas computed separately from the drawing, so they drift apart.
- Calling a CrossPlay behaviour a bug without comparing first. If PocketPlay matches CrossPlay, keep it and tell the user, as was done with D&Diagrams' `NEXT`.

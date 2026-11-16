---
name: new-game
description: Add a game to PocketPlay, either new or ported from another MIT-licensed project such as CrossPlay or touch-licorice-apps. Use whenever the user asks to create, add, port, copy or bring over a game, or to restyle a game's screens.
---

# Adding a game to PocketPlay

The rules live in the repo, not here. `CLAUDE.md` is already in your context, so don't read it again; follow its **Games**, **Storage**, and **Porting a game** sections. `CLAUDE.local.md` says where the source projects are. Read these first, in full, yourself rather than through subagents:

1. `src/games/README.md`: the folder layout, the boundaries, and how to list a game.
2. `src/games/DESIGN.md`: how a screen is built, and which `ui::` calls to use.
3. `tools/preview/README.md`: how to see the screens on the PC.

`src/games/dungeon/` is the worked example of a port, and `src/games/tictactoe/` is the smallest game.

## Steps

1. **Understand the source.** For a port, read the source game's code, its tests, any docs and any screenshot. For CrossPlay those are `src/apps_local/<name>/`, `host-tests/<name>/`, `docs/apps/<name>.md` and `site/assets/shots/<name>.png`; for touch-licorice-apps, `apps/<name>/`. For a new game, agree the rules with the user first.
2. **Ask before writing**, with AskUserQuestion, about anything that changes what gets built: the game's `name()` (it's saved in `session.sav`, so it's hard to change later), and any feature of the source that needs something PocketPlay lacks, such as multiplayer, the network or a second font.
3. **List every difference from the source for the user, before building**, as CLAUDE.md requires. Include what PocketPlay does that the source doesn't: the `BACK` button, saving on every change, byte-by-byte packing, `Nothing` for taps that change nothing, and the refresh choices.
4. **Shared changes come first, in their own commit.** If the game needs something in `core/` or `ui/`, add and commit that separately, so game branches don't conflict over it.
5. **Copy the rules and test unchanged except for includes**, with the source's licence beside them as `LICENSE-<source>.txt`. Add a save module and its test if the game saves. Add `tests/<name>/run.sh`, which takes its flags from `tests/flags.sh`.
6. **Write the screens and the `Game`** on `core/` and `ui/` only, following DESIGN.md. Add the icon, `README.md` for players (rules, getting around, saves, and a one-line credit; see `src/games/wordle/README.md`), and one line in `game_list.inc`. Add a line to the root README's game list, and raise the count in its opening ("a home page of N games") to the number of `GAME(` lines in `game_list.inc`. For a port, also raise that paragraph's count of games from the source.
7. **Preview every screen.** Run `tools/preview/run.sh` and look at each PNG with the Read tool. For a port, compare it with the source's screenshot. Check that each tap prints the `TapResult` you expect, and fix anything that's cut off, overlapping or misaligned before going further. After a fix, look again only at the screens that fix could change, not at every PNG.
8. **Check and build:** run `scripts/check.sh` (only `CHECK-VERDICT: green` passes) and `./scripts/package.sh`.
9. **Hand over:** commit as CLAUDE.md's **Commits** paragraph describes (`feat(<name>): add …` or `feat(<name>): port … from …`), show the user the preview images, give them the PowerShell flash commands from CLAUDE.md, and list what can only be checked on the device. Don't push unless they ask.

## Mistakes made before

- Drawing type a size too big: rows are Size20, band titles Size30, and pill labels Size14.
- Forgetting that the header holds `BACK`: a long title has less room and drops a cut.
- A shortened name, or `fitText()` with "..", where the text has to be read in full.
- `CLEAR` or a refused tap returning `Redraw`, so the screen blinks for nothing.
- Hit areas computed separately from the drawing, so they drift apart.
- Calling a source game's behaviour a bug without comparing first. If PocketPlay matches the source, keep it and tell the user, as was done with D&Diagrams' `NEXT`.

# Apps

Apps are tools rather than games. The home page leads to them through the grid icon beside the Settings cog, which opens the APPS page, a list of rows. BACK in an app returns to that page.

```
apps/
  app.h          what every app must provide
  app_list.inc   the list of apps: one line each
  apps.h/.cpp    looks apps up from the list; no app-specific code
```

## What an app provides

`App` in `app.h` is smaller than `Game`: a name, an icon, `open()`, `onTap()`, `draw()`, the save-failed pair, and the Up and Down buttons. Apps get no holds, swipes, Wi-Fi, landscape or work between taps, because no app has needed them yet. When one does, add the call to `App` and to `shell.cpp` in a change of its own, as `Game` has them.

`TapResult` works as a game's does: `Nothing` for a tap that changes nothing, `Redraw` for a change, `RedrawFull` for a change big enough to clear ghosts, and `Leave` for BACK.

## Adding an app

1. Create `src/apps/<name>/`, with the folder name in lowercase and no spaces.
2. Implement `App` from `apps/app.h`, and define a function that returns it, such as `App& calculatorApp()`.
3. Add one line to `app_list.inc`: `APP(calculatorApp)`.
4. Add it to the root `README.md`'s list of apps.

PlatformIO compiles every file under `src/`, so the build needs no changes. The app then appears on the APPS page and reopens after waking.

A folder holds the same files as a game's (`src/games/README.md`): the screen, the rules and the save packing apart, a `README.md` for players, and the licence of anything copied in. Draw it as `src/games/DESIGN.md` describes, and preview it with `tools/preview/run.sh apps/<name> <function> …`.

## The boundaries

- **An app may use `core/`, `ui/` and `apps/app.h`**, never a game's or another app's files.
- **Name save files after the folder**, as games do. Apps and games share `/.pocketplay/`, so an app's folder name must not start with a game's folder name, or the other way round.
- **Save on every change**, following the save rules in `CLAUDE.md` under Storage.

The APPS page shows every app on one page, with no paging yet. Add paging, as the home page has, before the list grows past one page.

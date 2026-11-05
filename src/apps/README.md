# Apps

Reserved for apps: tools rather than games. There aren't any yet, so nothing here is compiled.

Apps follow the same layout and boundaries as [games](../games/README.md): one folder per app, `src/apps/<name>/`, with no includes between apps, one line per app in a list, and save files named after the folder.

The first app still has to decide two things:

- **What every app provides.** `Game` may not be enough, because an app might need regular updates or the Up and Down buttons, so write `app.h` around what that first app really needs.
- **How the home page reaches apps.** The home page lists games and Settings, and nothing leads to apps yet. `shell/` is the only code that needs to change.

# D&Diagrams

A nonogram whose clues are a dungeon. Each number beside a row or column is how many walls it holds, and the walls have to make a dungeon by five rules: every dead end holds a monster and every monster is in a dead end, every chest sits in a 3x3 room with one way in, corridors are one cell wide, and all the floor joins up. Each of the 64 dungeons has exactly one answer.

- **Menu:** the campaign map, eight tiers of eight. A cleared dungeon is a filled room, and a tier's number fills once all eight are cleared. Tapping a room picks it, and the brackets show which one. `PLAY` opens it, or `RESUME` if it has marks on it. `TUTORIAL` opens the guide.
- **Board:** a tap cycles a cell through wall, floor note and blank. A monster or a chest refuses. A clue fills black when its row or column holds exactly that many walls, and turns grey when it holds more. `CLEAR` empties the board.
- **Guide:** eight pages, each a rule, shown on the tutorial's own board a step further along. The tutorial is never a level.
- **Win:** the dungeon's name and the finished map, with a full refresh. `NEXT` opens the next unsolved dungeon.

**Saves:** `dungeon` in `/.pocketplay/`, 33 bytes: the dungeon in hand, its walls and floor notes, and which dungeons are solved. It's written on every change and never erased, because it holds the campaign. A finished board isn't kept: the save moves on to the next dungeon.

**Copied code:** `dungeon_rules.h` and `.cpp` are CrossPlay's `src/apps_local/dungeon/DungeonCore.h` and `.cpp`, and `dungeon_puzzles.h` is its `DungeonPuzzles.h`, each changed only in one include. `tests/dungeon/test_dungeon.cpp` is CrossPlay's test, changed only in its includes. `dungeon_guide.cpp` holds the guide pages from `DungeonScreens.cpp`, moved unchanged so the test needs no drawing code. The screens and game are ported from `DungeonScreens.cpp` and `DungeonActivity.cpp`, redrawn without CrossPlay's UI toolkit. `dungeon_art.h` holds CrossPlay's monster, chest and home-page icons, drawn from Lucide icons (ISC licence). All of it matched CrossPlay commit `fbeac762` when last compared, and it's under the MIT licence, in `LICENSE-CrossPlay.txt`.

**Differences from CrossPlay:**

- `BACK` is a button in the header, because PocketPlay has no edge swipe. It leaves the board, the win screen and the guide's first page for the menu, steps back a page in the guide, and leaves the game from the menu. Because `BACK` takes room in the header, a long title such as `D&DIAGRAMS` drops from Size30 to Size20, and the count beside it is Size20.
- It saves on every mark. CrossPlay saves every 12 taps, so a power cut there loses up to 12 marks.
- The save is packed byte by byte rather than written as a struct, and a CrossPlay save isn't imported.
- A tap that changes nothing draws nothing: on a monster or a chest, on the picked room, or `CLEAR` on an empty board. CrossPlay redraws for each.
- Opening a board is a full refresh, as starting a game is everywhere in PocketPlay. CrossPlay uses a fast one.
- There are no button hints.

**Kept from CrossPlay as it is:** `NEXT` on the win screen is meant to dim once every dungeon is cleared, but the test counts the tutorial among the dungeons, so it never dims. After the last one, `NEXT` reopens dungeon 64.

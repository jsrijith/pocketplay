#!/usr/bin/env bash
# Draws the screenshot in every game's and app's README: a screen of it being played, made by
# tapping through the real game in the preview (README.md here). Run it again after changing a
# screen, and commit the PNGs it writes.
#
#   tools/preview/screenshots.sh              every game and app
#   tools/preview/screenshots.sh sudoku wordle  only these folders
#
# Each line below is a folder, its function, and the commands that reach the screen to show.
# The screenshot is taken after the last command, into <folder>/screenshot.png. Games that read
# downloaded or card data get made-up data from screenshot_data.py. frame.py puts a thin black
# border around each one, so a white screen doesn't fade into a white page.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
HERE="$ROOT/tools/preview"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
python3 -I "$HERE/screenshot_data.py" "$WORK/data"
DATA="$WORK/data"

SHOTS=(
  "authoritytracker authorityTrackerGame tap:240,372 tap:240,372 tap:84,692 tap:392,628"
  "bingocaller bingoCallerGame tap:240,204 tap:318,758 tap:318,758 tap:318,758 tap:318,758 tap:318,758 tap:318,758 tap:318,758"
  "chesspuzzles chessPuzzlesGame"
  "connectfour connectFourGame tap:240,686 tap:242,340 update tap:306,340 update tap:178,340 update"
  "connections connectionsGame tap:150,160 tap:297,218 tap:182,218 tap:67,333 tap:182,563 tap:395,758 tap:297,448 tap:182,448"
  "crowncrest crownCrestGame tap:400,756"
  "diceroller diceRollerGame tap:355,756 tap:355,756 tap:355,756"
  "dungeon dungeonGame tap:240,756"
  "gridgambit gridGambitGame update"
  "islandmaps islandMapsGame"
  "kakuro kakuroGame tap:182,758 tap:182,758 tap:182,758 tap:276,336"
  "knucklebones knucklebonesGame tap:240,686 tap:164,570 update tap:240,570 update tap:164,570 update"
  "lastpiece lastPieceGame tap:81,248"
  "lightsout lightsOutGame tap:158,354 tap:326,522"
  "minesweeper minesweeperGame tap:240,686 tap:240,400"
  "nonogram nonogramGame tap:113,380 tap:150,380 tap:187,380 tap:224,380 tap:261,380 tap:298,380 tap:335,380 tap:372,380 tap:409,380 tap:446,380 tap:113,417 tap:150,417 tap:187,417 tap:224,417 tap:261,417 tap:298,417 tap:335,417 tap:372,417 tap:409,417 tap:446,417"
  "oneclue oneClueGame"
  "pegsolitaire pegSolitaireGame tap:240,310 tap:240,433 tap:364,372 tap:240,372"
  "polyominopanic polyominoPanicGame tap:240,758"
  "queens queensGame tap:240,758 tap:182,758 tap:182,758 tap:182,758"
  "sokoban sokobanGame swipe:240,500,240,300 swipe:240,300,240,500 swipe:200,400,350,400"
  "solitaire solitaireGame tap:166,428"
  "sudoku sudokuGame tap:218,604 update tap:342,752 tap:342,752"
  "suguru suguruGame tap:153,297"
  "symboldice symbolDiceGame tap:60,758"
  "textadv textAdventuresGame tap:240,140 update"
  "thinkalike thinkAlikeGame"
  "tictactoe ticTacToe tap:240,450 tap:100,320 tap:380,320"
  "wordladder wordLadderGame tap:86,758 tap:86,758"
  "wordle wordleGame tap:150,150 tap:194,746 tap:171,587 tap:59,664 tap:329,746 tap:127,587 tap:47,746 tap:104,664 tap:419,664 tap:59,664 tap:216,587 tap:127,587 tap:47,746 tap:441,587 tap:419,664"
  "apps/calculator calculatorApp tap:67,632 tap:182,632 tap:412,422 tap:297,632"
  "apps/pocketbgg pocketBggApp"
)

export PREVIEW_BLOB_wordle_answers="$DATA/wordle-answers"
export PREVIEW_BLOB_wordle_words="$DATA/wordle-words"
export PREVIEW_BLOB_connections_pack="$DATA/connections-pack"
export PREVIEW_BLOB_pocketbgg_list="$DATA/pocketbgg-list"
export PREVIEW_CARD="$DATA/card"

failed=0
for line in "${SHOTS[@]}"; do
  read -r folder function commands <<<"$line"
  if [ "$#" -gt 0 ] && ! printf '%s\n' "$@" | grep -qx "$folder"; then continue; fi
  case "$folder" in
    apps/*) target="$ROOT/src/$folder" ;;
    *) target="$ROOT/src/games/$folder" ;;
  esac
  out="$WORK/$(basename "$folder")"
  # shellcheck disable=SC2086
  if "$HERE/run.sh" "$folder" "$function" "$out" $commands shot:screenshot >"$out.log" 2>&1; then
    python3 -I "$HERE/frame.py" "$out/screenshot.png"
    cp "$out/screenshot.png" "$target/screenshot.png"
    echo "ok    $folder"
  else
    echo "FAIL  $folder (see the log below)"
    cat "$out.log"
    failed=1
  fi
done
exit "$failed"

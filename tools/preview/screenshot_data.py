"""Writes the made-up data the README screenshots need into a folder, using only the standard library.

usage: python3 -I screenshot_data.py OUT_DIR

Wordle and Connections normally download their puzzles, and Last Piece and Think Alike read files
the player copies onto the card. None of that is in the repo, so screenshots.sh plays them with
this instead:

  wordle-answers     every day's answer, the same word each day, so the screenshot never changes
  wordle-words       the guess list: the answer and the guesses screenshots.sh types
  connections-pack   one puzzle, in the pack format of connections_pack.h
  pocketbgg-list     a made-up BGG collection of classic games, in pocketbgg_save.h's version 2,
                     with no cover URLs, so no box art is drawn
  card/lastpiece/    Last Piece's example challenges, as a player would copy them
  card/thinkalike/   Think Alike's starter questions
"""
import os
import shutil
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

WORDLE_ANSWER = "PLANT"
WORDLE_WORDS = ["CRANE", "SLATE", "PLANT", "PLAIN"]
WORDLE_DAYS = 6000  # from 19 June 2021, well past any date the preview runs on

# A puzzle made up for the screenshot: (level, name, four words).
CONNECTIONS = [
    (0, "FRUIT", ["APPLE", "PEAR", "PLUM", "FIG"]),
    (1, "THINGS WITH KEYS", ["PIANO", "LOCK", "MAP", "KEYBOARD"]),
    (2, "CARD GAMES", ["SNAP", "RUMMY", "BRIDGE", "POKER"]),
    (3, "___BALL", ["FOOT", "BASKET", "SNOW", "EYE"]),
]
CONNECTIONS_DATE = 20260101

# A made-up collection for PocketBGG: (BGG id, name, year, plays, min and max players, minutes,
# last played as YYYYMMDD or 0). All owned, and in the order a sync saves them: by name.
POCKETBGG_GAMES = [
    (2397, "Backgammon", -3000, 41, 2, 2, 30, 20261108),
    (2083, "Checkers", 1150, 27, 2, 2, 30, 20261021),
    (171, "Chess", 1475, 64, 2, 2, 60, 20261117),
    (5432, "Chinese Checkers", 1892, 0, 2, 6, 30, 0),
    (188, "Go", -2200, 12, 2, 2, 90, 20260930),
    (2448, "Mancala", 700, 9, 2, 2, 15, 20260614),
    (3886, "Nine Men's Morris", -1400, 3, 2, 2, 20, 20260302),
    (2136, "Pachisi", 400, 6, 2, 4, 60, 20260805),
]
POCKETBGG_SYNCED = 1794996000  # 18 November 2026, 10:00 UTC


def text16(text):
    data = text.encode("ascii")
    return struct.pack("<H", len(data)) + data


def pocketbgg_list():
    data = struct.pack("<BIH", 2, POCKETBGG_SYNCED, len(POCKETBGG_GAMES))
    for i, (gid, name, year, plays, low, high, minutes, last) in enumerate(POCKETBGG_GAMES):
        # id, collId, sortSkip, year, plays, min and max players, play time, status (owned), last played
        data += struct.pack("<IIBhHBBHHI", gid, 1000 + i, 0, year, plays, low, high, minutes, 1, last)
        data += text16(name) + text16("") + text16("")
    return data


def string(text):
    data = text.encode("ascii")
    return bytes([len(data)]) + data


def connections_pack():
    record = struct.pack("<HI", 1, CONNECTIONS_DATE)
    for level, name, words in CONNECTIONS:
        record += bytes([level]) + string(name) + b"".join(string(w) for w in words)
    index = struct.pack("<IHH", 0x31505843, 1, 0) + struct.pack("<II", CONNECTIONS_DATE, 0)
    # The blob is the index's length, then the index, then the records.
    return struct.pack("<I", len(index)) + index + record


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "wordle-answers"), "wb") as f:
        f.write(WORDLE_ANSWER.encode("ascii") * WORDLE_DAYS)
    with open(os.path.join(out, "wordle-words"), "wb") as f:
        f.write("".join(sorted(WORDLE_WORDS)).encode("ascii"))
    with open(os.path.join(out, "connections-pack"), "wb") as f:
        f.write(connections_pack())
    with open(os.path.join(out, "pocketbgg-list"), "wb") as f:
        f.write(pocketbgg_list())
    games = os.path.join(ROOT, "src", "games")
    lastpiece = os.path.join(out, "card", "lastpiece")
    thinkalike = os.path.join(out, "card", "thinkalike")
    os.makedirs(lastpiece, exist_ok=True)
    os.makedirs(thinkalike, exist_ok=True)
    shutil.copy(os.path.join(games, "lastpiece", "example.txt"), os.path.join(lastpiece, "beginner.txt"))
    shutil.copy(os.path.join(games, "lastpiece", "example-solutions.txt"), os.path.join(lastpiece, "beginner-solutions.txt"))
    shutil.copy(os.path.join(games, "thinkalike", "questions.txt"), os.path.join(thinkalike, "questions.txt"))


main()

#include "connections_fit.h"

#include <climits>
#include <cstdio>
#include <cstring>

namespace connectionsfit {

int spanWidth(Font font, const char* text, int begin, int end) {
  const int span = end - begin;
  if (span <= 0) return 0;
  if (span >= MAX_LINE_CHARS) return INT_MAX;
  char buffer[MAX_LINE_CHARS] = {};
  memcpy(buffer, text + begin, static_cast<size_t>(span));
  buffer[span] = '\0';
  return textWidth(font, buffer);
}

bool breakToFit(Font font, const char* text, int width, int maxLines, bool allowMidWord, bool preferSpaces, Lines& out) {
  out.count = 0;
  if (text == nullptr || width <= 0) return false;
  const int length = static_cast<int>(strlen(text));
  if (length >= MAX_LINE_CHARS) return false;
  const int cap = maxLines < MAX_LINES ? maxLines : MAX_LINES;
  int at = 0;
  while (at < length) {
    while (at < length && text[at] == ' ') at++;
    if (at >= length) break;
    if (out.count >= cap) return false;
    int take = length - at;
    if (spanWidth(font, text, at, at + take) > width) {
      // The longest prefix that fits, found by halving.
      int low = 1;
      int high = take;
      while (low < high) {
        const int middle = (low + high + 1) / 2;
        if (spanWidth(font, text, at, at + middle) <= width) {
          low = middle;
        } else {
          high = middle - 1;
        }
      }
      take = low;
      if (spanWidth(font, text, at, at + take) > width) return false;

      // Back to the phrase's own space when one is in reach, measured again after the
      // move so the guarantee doesn't rest on width growing with length.
      bool atSpace = false;
      for (int i = preferSpaces ? take : 0; i > 0; i--) {
        if (text[at + i - 1] == ' ') {
          if (i - 1 > 0 && spanWidth(font, text, at, at + i - 1) <= width) {
            take = i - 1;
            atSpace = true;
          }
          break;
        }
      }
      if (!atSpace && !allowMidWord) return false;
      if (take <= 0) return false;
    }
    out.begin[out.count] = static_cast<uint8_t>(at);
    out.end[out.count] = static_cast<uint8_t>(at + take);
    out.count++;
    at += take;
  }
  return true;
}

void balanceBreaks(Font font, const char* text, int width, int maxLines, Lines& io) {
  if (io.count < 2) return;
  const int length = static_cast<int>(strlen(text));
  const int cap = maxLines < MAX_LINES ? maxLines : MAX_LINES;
  // Even shares of the same line count don't always fit, so one more line at a time is
  // allowed: three even lines beat two lopsided ones.
  for (int parts = io.count; parts <= cap; parts++) {
    int bound[MAX_LINES + 1] = {};
    bound[parts] = length;
    for (int i = 1; i < parts; i++) bound[i] = length * i / parts;
    // A space up to three characters away is taken instead.
    for (int i = 1; i < parts; i++) {
      for (int slack = 0; slack <= 3; slack++) {
        if (bound[i] - slack > bound[i - 1] && text[bound[i] - slack] == ' ') {
          bound[i] -= slack;
          break;
        }
        if (bound[i] + slack < bound[i + 1] && text[bound[i] + slack] == ' ') {
          bound[i] += slack;
          break;
        }
      }
    }

    Lines even;
    even.count = parts;
    bool fits = true;
    for (int i = 0; i < parts && fits; i++) {
      int begin = bound[i];
      int end = bound[i + 1];
      while (begin < end && text[begin] == ' ') begin++;
      while (end > begin && text[end - 1] == ' ') end--;
      if (end <= begin || spanWidth(font, text, begin, end) > width) {
        fits = false;
        break;
      }
      even.begin[i] = static_cast<uint8_t>(begin);
      even.end[i] = static_cast<uint8_t>(end);
    }
    if (fits) {
      io = even;
      return;
    }
  }
}

TileCut chooseTileCut(const char* const* words, int count, int innerWidth, int innerHeight) {
  for (const Font cut : CUTS) {
    int lines = innerHeight / lineHeight(cut);
    if (lines > TILE_LINES) lines = TILE_LINES;
    if (lines < 1) continue;
    bool all = true;
    Lines probe;
    for (int i = 0; i < count && all; i++) {
      all = breakToFit(cut, words[i], innerWidth, lines, false, true, probe);
    }
    if (all) return TileCut{cut, lines, false};
  }
  // No cut sets every word whole, so a word is cut, at the smallest cut: the fewest
  // pieces it can come apart into.
  const Font cut = CUTS[CUT_COUNT - 1];
  int lines = innerHeight / lineHeight(cut);
  if (lines > MAX_LINES) lines = MAX_LINES;
  if (lines < 1) lines = 1;
  return TileCut{cut, lines, true};
}

void layoutTile(const char* word, int innerWidth, const TileCut& cut, Lines& lines) {
  if (!breakToFit(cut.font, word, innerWidth, cut.maxLines, cut.mayBreakWords, true, lines)) {
    // chooseTileCut promised a fit, so this is a word no cut can hold. Packed as tightly
    // as possible, spaces ignored, so every character still arrives.
    breakToFit(cut.font, word, innerWidth, MAX_LINES, true, false, lines);
  }
  if (cut.mayBreakWords) balanceBreaks(cut.font, word, innerWidth, cut.maxLines, lines);
}

void layoutSolvedRow(const connections::Group& group, int width, int height, SolvedRow& row) {
  snprintf(row.words, sizeof(row.words), "%s, %s, %s, %s", group.members[0], group.members[1], group.members[2], group.members[3]);

  // Every pair of cuts, largest first, the name's cut stepping down last, first with words
  // whole, then letting one come apart.
  bool broke = false;
  row.fits = false;
  for (int pass = 0; pass < 2 && !row.fits; pass++) {
    const bool mayBreakWords = pass == 1;
    for (int n = 0; n < CUT_COUNT && !row.fits; n++) {
      for (int l = 0; l < CUT_COUNT && !row.fits; l++) {
        Lines name;
        Lines list;
        if (!breakToFit(CUTS[n], group.name, width, MAX_LINES, mayBreakWords, true, name)) continue;
        if (!breakToFit(CUTS[l], row.words, width, MAX_LINES, mayBreakWords, true, list)) continue;
        const int nameBlock = name.count * lineHeight(CUTS[n]);
        const int listBlock = list.count * lineHeight(CUTS[l]);
        if (nameBlock + listBlock > height) continue;
        row.nameFont = CUTS[n];
        row.listFont = CUTS[l];
        row.name = name;
        row.list = list;
        row.nameHeight = nameBlock;
        row.listHeight = listBlock;
        broke = mayBreakWords;
        row.fits = true;
      }
    }
  }
  if (!row.fits) {
    // Both whole at the smallest cut, though taller than the row: a block that runs
    // long can be seen, where a shortened group name can't.
    row.nameFont = CUTS[CUT_COUNT - 1];
    row.listFont = CUTS[CUT_COUNT - 1];
    breakToFit(row.nameFont, group.name, width, MAX_LINES, true, false, row.name);
    breakToFit(row.listFont, row.words, width, MAX_LINES, true, false, row.list);
    row.nameHeight = row.name.count * lineHeight(row.nameFont);
    row.listHeight = row.list.count * lineHeight(row.listFont);
    broke = true;
  }
  if (broke) {
    balanceBreaks(row.nameFont, group.name, width, MAX_LINES, row.name);
    balanceBreaks(row.listFont, row.words, width, MAX_LINES, row.list);
  }
}

}  // namespace connectionsfit

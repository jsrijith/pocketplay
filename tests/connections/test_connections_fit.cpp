// Every board of the published archive through the tile and solved-row layout, in every
// solve state, measured with the real Jersey metrics. Fails on a single line wider than
// its box, a character that doesn't arrive, or a word cut while a smaller cut would
// have held it. archive.idx and archive.dat are CrossPlay's host-tests/tilefit fixture,
// in the pack format connections_pack.h reads.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "connections_fit.h"
#include "connections_pack.h"

namespace fit = connectionsfit;

namespace {

int checksRun = 0;
int checksFailed = 0;
int failuresShown = 0;

void check(bool condition, const char* what, const std::string& detail) {
  checksRun++;
  if (condition) return;
  checksFailed++;
  if (failuresShown++ < 20) printf("FAIL %s: %s\n", what, detail.c_str());
}

std::vector<uint8_t> readFile(const char* path) {
  std::vector<uint8_t> data;
  FILE* file = fopen(path, "rb");
  if (file == nullptr) return data;
  int c = 0;
  while ((c = fgetc(file)) != EOF) data.push_back(static_cast<uint8_t>(c));
  fclose(file);
  return data;
}

bool readBytes(void* context, uint32_t offset, void* out, uint32_t size) {
  const auto* data = static_cast<const std::vector<uint8_t>*>(context);
  if (static_cast<size_t>(offset) + size > data->size()) return false;
  memcpy(out, data->data() + offset, size);
  return true;
}

std::string withoutSpaces(const char* text, int begin, int end) {
  std::string out;
  for (int i = begin; i < end; i++) {
    if (text[i] != ' ') out += text[i];
  }
  return out;
}

// Each line inside width, every non-space character present in order, and no break in
// the middle of a word unless that was allowed.
void checkLines(const char* what, Font font, const char* text, int width, const fit::Lines& lines, bool mayBreakWords, int& midWordBreaks) {
  std::string joined;
  for (int i = 0; i < lines.count; i++) {
    const int lineWidth = fit::spanWidth(font, text, lines.begin[i], lines.end[i]);
    check(lineWidth <= width, what, std::string(text) + " line " + std::to_string(i) + " is " + std::to_string(lineWidth) + " px of " + std::to_string(width));
    joined += withoutSpaces(text, lines.begin[i], lines.end[i]);
    if (i > 0) {
      const int at = lines.begin[i];
      const bool atSpace = at > 0 && (text[at - 1] == ' ' || text[lines.end[i - 1]] == ' ');
      if (!atSpace) {
        midWordBreaks++;
        check(mayBreakWords, what, std::string(text) + " is cut mid-word though a cut could set it whole");
      }
    }
  }
  check(joined == withoutSpaces(text, 0, static_cast<int>(strlen(text))), what, std::string(text) + " lost characters");
}

}  // namespace

int main(int argc, char** argv) {
  const std::string folder = argc > 1 ? argv[1] : ".";
  const std::vector<uint8_t> index = readFile((folder + "/archive.idx").c_str());
  const std::vector<uint8_t> data = readFile((folder + "/archive.dat").c_str());
  connections::PackReader pack;
  if (!pack.open(readBytes, const_cast<std::vector<uint8_t>*>(&index), const_cast<std::vector<uint8_t>*>(&data), static_cast<uint32_t>(index.size()))) {
    printf("FAIL can't open the archive fixture in %s\n", folder.c_str());
    return 1;
  }

  int boardsAtCut[fit::CUT_COUNT] = {};
  int boardsBreakingWords = 0;
  int midWordBreaks = 0;
  int solvedRowsTooTall = 0;
  int solvedAtCut[fit::CUT_COUNT][fit::CUT_COUNT] = {};

  for (int p = 0; p < pack.count(); p++) {
    connections::Puzzle puzzle;
    check(pack.readPuzzle(p, puzzle), "read", std::to_string(p));

    // The tiles left on the board for each set of solved groups.
    for (int solved = 0; solved < (1 << connections::kGroups); solved++) {
      const char* words[connections::kTiles];
      int count = 0;
      for (int g = 0; g < connections::kGroups; g++) {
        if (solved & (1 << g)) continue;
        for (int m = 0; m < connections::kMembers; m++) words[count++] = puzzle.groups[g].members[m];
      }
      if (count == 0) continue;

      const fit::TileCut cut = fit::chooseTileCut(words, count, fit::TILE_INNER_WIDTH, fit::TILE_INNER_HEIGHT);
      if (solved == 0) {
        for (int c = 0; c < fit::CUT_COUNT; c++) {
          if (fit::CUTS[c] == cut.font) boardsAtCut[c]++;
        }
        if (cut.mayBreakWords) boardsBreakingWords++;
      }
      for (int i = 0; i < count; i++) {
        fit::Lines lines;
        fit::layoutTile(words[i], fit::TILE_INNER_WIDTH, cut, lines);
        checkLines("tile", cut.font, words[i], fit::TILE_INNER_WIDTH, lines, cut.mayBreakWords, midWordBreaks);
        check(lines.count * lineHeight(cut.font) <= fit::TILE_INNER_HEIGHT, "tile height", std::string(words[i]) + " needs " + std::to_string(lines.count) + " lines");
      }

      // A word may only be cut when no cut holds every word on the board whole.
      if (cut.mayBreakWords) {
        for (const Font smaller : fit::CUTS) {
          const int lines = fit::TILE_INNER_HEIGHT / lineHeight(smaller) < fit::TILE_LINES ? fit::TILE_INNER_HEIGHT / lineHeight(smaller) : fit::TILE_LINES;
          bool all = lines > 0;
          fit::Lines probe;
          for (int i = 0; i < count && all; i++) all = fit::breakToFit(smaller, words[i], fit::TILE_INNER_WIDTH, lines, false, true, probe);
          check(!all, "tile cut", "a board breaks words though a cut sets them all whole");
        }
      }
    }

    for (const connections::Group& group : puzzle.groups) {
      fit::SolvedRow row;
      fit::layoutSolvedRow(group, fit::SOLVED_INNER_WIDTH, fit::ROW_HEIGHT, row);
      if (!row.fits) solvedRowsTooTall++;
      check(row.fits, "solved row height", std::string(group.name) + " / " + row.words);
      int unused = 0;
      checkLines("group name", row.nameFont, group.name, fit::SOLVED_INNER_WIDTH, row.name, true, unused);
      checkLines("group words", row.listFont, row.words, fit::SOLVED_INNER_WIDTH, row.list, true, unused);
      int n = 0;
      int l = 0;
      for (int c = 0; c < fit::CUT_COUNT; c++) {
        if (fit::CUTS[c] == row.nameFont) n = c;
        if (fit::CUTS[c] == row.listFont) l = c;
      }
      solvedAtCut[n][l]++;
    }
  }

  printf("%d boards. Full boards set at Size20 %d, Size14 %d, Size10 %d, cutting a word %d (%d mid-word breaks over all solve states).\n", pack.count(), boardsAtCut[0], boardsAtCut[1], boardsAtCut[2], boardsBreakingWords, midWordBreaks);
  printf("Solved rows, name/list cuts: 20/20 %d, 20/14 %d, 14/14 %d, 14/10 %d, 10/10 %d, other %d, too tall %d.\n", solvedAtCut[0][0], solvedAtCut[0][1], solvedAtCut[1][1], solvedAtCut[1][2], solvedAtCut[2][2],
         pack.count() * connections::kGroups - solvedAtCut[0][0] - solvedAtCut[0][1] - solvedAtCut[1][1] - solvedAtCut[1][2] - solvedAtCut[2][2], solvedRowsTooTall);
  printf("%d checks, %d failed\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}

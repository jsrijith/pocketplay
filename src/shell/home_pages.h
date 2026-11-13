#pragma once

// The home page's paging, kept free of drawing so tests/home/ can check it on the PC.
// A page is always found from a game's position, never stored as a number: rows per
// page could change, and then a stored page number would point at different games.
namespace homePages {

constexpr int ROWS_PER_PAGE = 10;

constexpr int pageCount(int games) { return games <= 0 ? 1 : (games + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE; }

constexpr int pageOf(int index) { return index < 0 ? 0 : index / ROWS_PER_PAGE; }

constexpr int firstOnPage(int page) { return page * ROWS_PER_PAGE; }

// How many games the page holds. The last page can be short.
constexpr int rowsOnPage(int page, int games) {
  const int left = games - firstOnPage(page);
  return left < 0 ? 0 : (left < ROWS_PER_PAGE ? left : ROWS_PER_PAGE);
}

// One step, wrapping past the last page to the first and back. The header's counter
// and the page marks show where a wrap landed.
constexpr int stepWrapped(int page, int delta, int games) {
  const int count = pageCount(games);
  return ((page + delta) % count + count) % count;
}

// The page to open on, from the saved game's position, or -1 if that game is gone.
// A gone game was last seen somewhere on the list, so the nearest place left is the
// last page (CrossPlay's rule), not the top.
constexpr int pageForSaved(int index, int games) {
  if (index < 0) return pageCount(games) - 1;
  const int page = pageOf(index);
  const int last = pageCount(games) - 1;
  return page > last ? last : page;
}

}  // namespace homePages

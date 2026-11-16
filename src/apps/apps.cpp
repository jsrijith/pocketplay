#include "apps.h"

#include <cstring>

#define APP(function) App& function();
#include "app_list.inc"
#undef APP

namespace apps {

namespace {

// The nullptr at the end keeps the array legal while the list is empty: C++ has no
// zero-length arrays. It's never returned, because count() leaves it out.
App* entry(int index) {
  static App* const APPS[] = {
#define APP(function) &function(),
#include "app_list.inc"
#undef APP
      nullptr,
  };
  return APPS[index];
}

constexpr int APP_COUNT = 0
#define APP(function) +1
#include "app_list.inc"
#undef APP
    ;

}  // namespace

int count() { return APP_COUNT; }

App& at(int index) { return *entry(index); }

App* find(const char* name) {
  for (int i = 0; i < APP_COUNT; i++) {
    if (strcmp(entry(i)->name(), name) == 0) return entry(i);
  }
  return nullptr;
}

}  // namespace apps

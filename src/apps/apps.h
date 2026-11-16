#pragma once

#include "app.h"

// The apps on the APPS page, in the order they're listed.
namespace apps {

int count();
App& at(int index);

// Returns nullptr if no app has this name.
App* find(const char* name);

}  // namespace apps

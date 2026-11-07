#pragma once

// The firmware's version, from scripts/version.py: VERSION's number, the build (the commit
// count) and the commit, with a + if it had changes not yet committed. A build without the
// script, such as the PC tests and the preview, shows DEV.
#ifndef POCKETPLAY_VERSION
#define POCKETPLAY_VERSION "DEV"
#endif
#ifndef POCKETPLAY_BUILD
#define POCKETPLAY_BUILD 0
#endif
#ifndef POCKETPLAY_COMMIT
#define POCKETPLAY_COMMIT "LOCAL"
#endif

namespace version {

constexpr const char* NUMBER = POCKETPLAY_VERSION;
constexpr int BUILD = POCKETPLAY_BUILD;
constexpr const char* COMMIT = POCKETPLAY_COMMIT;

}  // namespace version

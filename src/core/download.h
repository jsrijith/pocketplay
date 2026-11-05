#pragma once

#include <cstddef>
#include <cstdint>

// HTTPS downloads, one at a time. A download runs in its own task so loop() keeps
// reading input. That task only hands bytes to the caller's write function, which
// must keep them in memory: the card and panel share a bus with no lock, so anything
// going to the card waits until the download has ended.
namespace download {

enum class State { Idle, Running, Done, Failed };

struct Sink {
  void* context;
  bool (*write)(void* context, const uint8_t* data, size_t size);  // return false to stop
  void (*restart)(void* context);                                  // drop all bytes so far
};

// Returns false if Wi-Fi isn't connected or a download is already running. The URL and
// the sink must stay valid until the download has ended.
bool start(const char* url, const Sink& sink);

State state();
int httpStatus();  // from the last download; 0 if no server answered

// Asks a running download to stop. state() changes to Failed once it has.
void requestCancel();

// For the sleep path. Ends any download, waiting briefly for it to stop cleanly.
void stopForSleep();

}  // namespace download

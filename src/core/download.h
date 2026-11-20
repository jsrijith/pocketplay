#pragma once

#include <cstddef>
#include <cstdint>

// HTTPS downloads, one at a time. A download runs in its own task so loop() keeps
// reading input. That task only hands bytes to the caller's write function, which
// must keep them in memory: the card and panel share a bus with no lock, so anything
// going to the card waits until the download has ended.
//
// By default the server's certificate isn't checked, which is fine for public files
// fetched with nothing private sent. Anything private, such as a token, needs
// Options::rootCa, so it only ever reaches the server it's meant for.
namespace download {

enum class State { Idle, Running, Done, Failed };

struct Sink {
  void* context;
  bool (*write)(void* context, const uint8_t* data, size_t size);  // return false to stop
  void (*restart)(void* context);                                  // drop all bytes so far
};

struct Options {
  // A root certificate in PEM form. With one, the server's certificate chain must lead
  // to it, or the connection fails before anything is sent. Without one, nothing is
  // checked.
  const char* rootCa = nullptr;
  // One extra request header, such as "Authorization" and "Bearer ...". It's sent only
  // while the request stays on the starting URL's scheme, host and port, so a redirect
  // never carries it to another server. The value is never logged.
  const char* headerName = nullptr;
  const char* headerValue = nullptr;
};

// Returns false if Wi-Fi isn't connected or a download is already running. The URL, the
// sink and everything the options point to must stay valid until the download has ended.
bool start(const char* url, const Sink& sink, const Options& options = Options{});

State state();
// From the last download; 0 if no server answered, which includes a certificate that
// failed the check: the serial log then shows wolfSSL's error code.
int httpStatus();

// Asks a running download to stop. state() changes to Failed once it has.
void requestCancel();

// For the sleep path. Ends any download, waiting briefly for it to stop cleanly.
void stopForSleep();

}  // namespace download

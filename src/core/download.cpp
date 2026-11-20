#include "download.h"

#include <Arduino.h>
#include <ResumableFetch.h>
#include <esp_wifi.h>

#include <atomic>

#include "network.h"

// wolfSSL's debug output is sent here. We don't log it.
extern "C" void wolfSSL_Arduino_Serial_Print(const char* const) {}

namespace download {

namespace {

// TLS needs far more stack than our other code, and this task is the only place it runs.
constexpr uint32_t TASK_STACK_BYTES = 16384;
constexpr UBaseType_t TASK_PRIORITY = 1;
constexpr int TIMEOUT_MS = 60000;
constexpr uint32_t STOP_WAIT_MS = 5000;
constexpr const char* USER_AGENT = "PocketPlay";

std::atomic<State> currentState{State::Idle};
std::atomic<bool> cancelRequested{false};
std::atomic<int> lastStatus{0};

TaskHandle_t task = nullptr;
const char* currentUrl = nullptr;
Sink currentSink{};
Options currentOptions{};

// Modem sleep adds seconds to a TLS handshake and can stall a transfer.
class FullPowerRadio {
 public:
  FullPowerRadio() {
    if (esp_wifi_get_ps(&previous) != ESP_OK) previous = WIFI_PS_MIN_MODEM;
    esp_wifi_set_ps(WIFI_PS_NONE);
  }
  ~FullPowerRadio() { esp_wifi_set_ps(previous); }

 private:
  wifi_ps_type_t previous = WIFI_PS_MIN_MODEM;
};

bool fetch() {
  FullPowerRadio fullPower;

  freeink::FetchSink sink;
  sink.write = [](const uint8_t* data, size_t size) { return currentSink.write(currentSink.context, data, size); };
  sink.rewind = [] {
    currentSink.restart(currentSink.context);
    return true;
  };

  // Without a root, the answers aren't secret and nothing private is sent, so like
  // CrossPlay this doesn't check the server's certificate. sameOrigin is false once a
  // redirect has left the starting server, and the header stays behind then.
  const auto configure = [](freeink::SecureHttpClient& http, bool sameOrigin) {
    http.setTimeout(TIMEOUT_MS);
    if (currentOptions.rootCa != nullptr) {
      http.setCACert(currentOptions.rootCa);
    } else {
      http.setInsecure();
    }
    http.setUserAgent(USER_AGENT);
    if (sameOrigin && currentOptions.headerName != nullptr && currentOptions.headerValue != nullptr) {
      http.addHeader(currentOptions.headerName, currentOptions.headerValue);
    }
  };

  const freeink::FetchResult result = freeink::fetchResumable(currentUrl, freeink::FetchOptions{}, configure, sink, [] { return cancelRequested.load(); });

  lastStatus = result.status < 0 ? 0 : result.status;
  Serial.printf("Fetched %u bytes, status %d, from %s (certificate %s)\n", static_cast<unsigned>(result.bytes), result.status,
                currentUrl, currentOptions.rootCa != nullptr ? "checked" : "not checked");
  return result.complete && !result.aborted && !result.stopped && result.status >= 200 && result.status < 300;
}

void runTask(void*) {
  const bool succeeded = fetch();
  currentState = succeeded ? State::Done : State::Failed;
  task = nullptr;
  vTaskDelete(nullptr);
}

}  // namespace

bool start(const char* url, const Sink& sink, const Options& options) {
  if (currentState == State::Running) return false;

  // Entering the socket layer with Wi-Fi never started crashes the device.
  if (network::state() != network::State::Connected) {
    Serial.println("ERROR: no Wi-Fi for a download");
    return false;
  }

  currentUrl = url;
  currentSink = sink;
  currentOptions = options;
  cancelRequested = false;
  lastStatus = 0;
  currentState = State::Running;
  if (xTaskCreate(runTask, "download", TASK_STACK_BYTES, nullptr, TASK_PRIORITY, &task) != pdPASS) {
    Serial.println("ERROR: no memory for the download task");
    currentState = State::Failed;
    return false;
  }
  return true;
}

State state() { return currentState; }

int httpStatus() { return lastStatus; }

void requestCancel() { cancelRequested = true; }

void stopForSleep() {
  if (currentState != State::Running) return;

  cancelRequested = true;
  const uint32_t startMs = millis();
  while (currentState == State::Running && millis() - startMs < STOP_WAIT_MS) {
    delay(10);
  }

  // Waking resets the chip, so a task stuck in a socket call is safe to drop here,
  // and nowhere else: the radio is about to go off underneath it.
  if (currentState == State::Running && task != nullptr) {
    vTaskDelete(task);
    task = nullptr;
    currentState = State::Failed;
  }
}

}  // namespace download

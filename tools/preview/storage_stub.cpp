// The card, kept in memory for one run, so a game's saves work while it's previewed.
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core/storage.h"

namespace storage {

namespace {

struct Save {
  uint8_t version = 0;
  std::vector<uint8_t> data;
};

std::map<std::string, Save> saves;
std::map<std::string, std::vector<uint8_t>> blobs;

}  // namespace

bool begin() { return true; }
bool ready() { return true; }

bool save(const char* name, uint8_t version, const uint8_t* data, size_t size) {
  if (size > MAX_DATA_BYTES) return false;
  saves[name] = Save{version, std::vector<uint8_t>(data, data + size)};
  return true;
}

bool load(const char* name, uint8_t& version, uint8_t* data, size_t capacity, size_t& size) {
  const auto found = saves.find(name);
  if (found == saves.end() || found->second.data.size() > capacity) return false;
  version = found->second.version;
  size = found->second.data.size();
  if (size > 0) memcpy(data, found->second.data.data(), size);
  return true;
}

bool erase(const char* name) {
  saves.erase(name);
  return true;
}

void BlobDeleter::operator()(uint8_t* data) const { delete[] data; }

bool saveBlob(const char* name, const uint8_t* data, size_t size) {
  blobs[name] = std::vector<uint8_t>(data, data + size);
  return true;
}

Blob loadBlob(const char* name, size_t& size) {
  const auto found = blobs.find(name);
  if (found == blobs.end()) return Blob();
  size = found->second.size();
  Blob blob(new uint8_t[size > 0 ? size : 1]);
  if (size > 0) memcpy(blob.get(), found->second.data(), size);
  return blob;
}

}  // namespace storage

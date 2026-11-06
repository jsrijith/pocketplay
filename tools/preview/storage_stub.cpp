// The card, kept in memory for one run, so a game's saves work while it's previewed.
// A blob can be put on it before the game opens: PREVIEW_BLOB_<name>=<file> loads
// that file as the blob <name>, with any '_' in the name read as '-'.
#include <cstdio>
#include <cstdlib>
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

extern "C" char** environ;

bool begin() {
  static const char PREFIX[] = "PREVIEW_BLOB_";
  for (char** entry = environ; *entry != nullptr; entry++) {
    if (strncmp(*entry, PREFIX, sizeof(PREFIX) - 1) != 0) continue;
    const char* rest = *entry + sizeof(PREFIX) - 1;
    const char* equals = strchr(rest, '=');
    if (equals == nullptr) continue;
    std::string name(rest, equals);
    for (char& c : name) {
      if (c == '_') c = '-';
    }
    FILE* file = fopen(equals + 1, "rb");
    if (file == nullptr) {
      fprintf(stderr, "can't read %s for blob %s\n", equals + 1, name.c_str());
      continue;
    }
    std::vector<uint8_t> data;
    int c = 0;
    while ((c = fgetc(file)) != EOF) data.push_back(static_cast<uint8_t>(c));
    fclose(file);
    blobs[name] = data;
  }
  return true;
}
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

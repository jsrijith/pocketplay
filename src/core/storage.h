#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

// Small save files on the SD card, one per name, in /.pocketplay/. A power cut at
// any moment leaves either the old save or the new one, never a damaged one.
namespace storage {

constexpr size_t MAX_DATA_BYTES = 512;

// Mounts the card and creates the folder. Returns false if there's no usable card.
bool begin();
bool ready();

bool save(const char* name, uint8_t version, const uint8_t* data, size_t size);

// Returns false if there's no valid save, which is normal on first use.
bool load(const char* name, uint8_t& version, uint8_t* data, size_t capacity, size_t& size);

// Returns true once no save by this name is left on the card.
bool erase(const char* name);

// For data too big for a save, such as a word list. These work the same way, and the
// data lives in PSRAM. A blob named "x" is a separate file from a save named "x".
struct BlobDeleter {
  void operator()(uint8_t* data) const;
};
using Blob = std::unique_ptr<uint8_t[], BlobDeleter>;

bool saveBlob(const char* name, const uint8_t* data, size_t size);

// Returns nothing if there's no valid blob or no memory for it.
Blob loadBlob(const char* name, size_t& size);

}  // namespace storage

#include "storage.h"

#include <Arduino.h>
#include <SDCardManager.h>
#include <esp_heap_caps.h>

#include <cstdio>
#include <cstring>

namespace storage {
namespace {

constexpr const char* DIRECTORY = "/.pocketplay";
constexpr size_t PATH_BYTES = 48;

// 'B', 'B', format, data version, 16-bit length, 32-bit checksum, least significant
// byte first. The checksum covers the version, the length and the data.
constexpr size_t HEADER_BYTES = 10;
constexpr uint8_t FORMAT = 1;

// 'B', 'L', format, 32-bit length, 32-bit checksum of the data.
constexpr size_t BLOB_HEADER_BYTES = 11;
constexpr uint8_t BLOB_FORMAT = 1;

bool mounted = false;

void makePath(char* path, const char* name, const char* extension) {
  snprintf(path, PATH_BYTES, "%s/%s.%s", DIRECTORY, name, extension);
}

uint32_t updateChecksum(uint32_t crc, const uint8_t* data, size_t size) {
  for (size_t i = 0; i < size; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
  }
  return crc;
}

uint32_t checksumOf(uint8_t version, uint16_t length, const uint8_t* data) {
  const uint8_t fields[3] = {version, static_cast<uint8_t>(length & 0xFF), static_cast<uint8_t>(length >> 8)};
  uint32_t crc = updateChecksum(0xFFFFFFFFu, fields, sizeof(fields));
  crc = updateChecksum(crc, data, length);
  return ~crc;
}

void makeHeader(uint8_t* header, uint8_t version, const uint8_t* data, size_t size) {
  const auto length = static_cast<uint16_t>(size);
  const uint32_t crc = checksumOf(version, length, data);
  header[0] = 'B';
  header[1] = 'B';
  header[2] = FORMAT;
  header[3] = version;
  header[4] = static_cast<uint8_t>(length & 0xFF);
  header[5] = static_cast<uint8_t>(length >> 8);
  for (int i = 0; i < 4; i++) {
    header[6 + i] = static_cast<uint8_t>(crc >> (8 * i));
  }
}

// Reads back what was just written. This catches a bad write, and because the card
// can't answer until it finishes writing, it's also safe to cut its power afterwards.
bool fileMatches(const char* path, const uint8_t* header, size_t headerBytes, const uint8_t* data, size_t size) {
  FsFile file = SdMan.open(path, O_RDONLY);
  if (!file || file.fileSize() != headerBytes + size) return false;

  uint8_t buffer[64];
  if (file.read(buffer, headerBytes) != static_cast<int>(headerBytes) || memcmp(buffer, header, headerBytes) != 0) return false;

  size_t offset = 0;
  while (offset < size) {
    const size_t chunk = min(size - offset, sizeof(buffer));
    if (file.read(buffer, chunk) != static_cast<int>(chunk) || memcmp(buffer, data + offset, chunk) != 0) return false;
    offset += chunk;
  }
  return true;
}

bool writeFile(const char* path, const uint8_t* header, size_t headerBytes, const uint8_t* data, size_t size) {
  FsFile file = SdMan.open(path, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file) return false;
  if (file.write(header, headerBytes) != headerBytes || file.write(data, size) != size) return false;
  return file.close();
}

bool readValid(const char* path, uint8_t& version, uint8_t* data, size_t capacity, size_t& size) {
  FsFile file = SdMan.open(path, O_RDONLY);
  if (!file) return false;

  uint8_t header[HEADER_BYTES];
  if (file.read(header, HEADER_BYTES) != static_cast<int>(HEADER_BYTES)) return false;
  if (header[0] != 'B' || header[1] != 'B' || header[2] != FORMAT) return false;

  const auto length = static_cast<uint16_t>(header[4] | (header[5] << 8));
  if (length > capacity || file.fileSize() != HEADER_BYTES + length) return false;
  if (file.read(data, length) != static_cast<int>(length)) return false;

  uint32_t stored = 0;
  for (int i = 0; i < 4; i++) {
    stored |= static_cast<uint32_t>(header[6 + i]) << (8 * i);
  }
  if (checksumOf(header[3], length, data) != stored) return false;

  version = header[3];
  size = length;
  return true;
}

uint32_t readUint32(const uint8_t* in) {
  uint32_t value = 0;
  for (int i = 0; i < 4; i++) {
    value |= static_cast<uint32_t>(in[i]) << (8 * i);
  }
  return value;
}

void writeUint32(uint8_t* out, uint32_t value) {
  for (int i = 0; i < 4; i++) {
    out[i] = static_cast<uint8_t>(value >> (8 * i));
  }
}

Blob readValidBlob(const char* path, size_t& size) {
  FsFile file = SdMan.open(path, O_RDONLY);
  if (!file) return nullptr;

  uint8_t header[BLOB_HEADER_BYTES];
  if (file.read(header, BLOB_HEADER_BYTES) != static_cast<int>(BLOB_HEADER_BYTES)) return nullptr;
  if (header[0] != 'B' || header[1] != 'L' || header[2] != BLOB_FORMAT) return nullptr;

  const uint32_t length = readUint32(header + 3);
  if (file.fileSize() != BLOB_HEADER_BYTES + length) return nullptr;

  // One extra byte, so an empty blob still returns a buffer.
  Blob data(static_cast<uint8_t*>(heap_caps_malloc(length + 1, MALLOC_CAP_SPIRAM)));
  if (!data) {
    Serial.printf("ERROR: no memory for %lu bytes from %s\n", static_cast<unsigned long>(length), path);
    return nullptr;
  }
  if (file.read(data.get(), length) != static_cast<int>(length)) return nullptr;
  if (~updateChecksum(0xFFFFFFFFu, data.get(), length) != readUint32(header + 7)) return nullptr;

  size = length;
  return data;
}

}  // namespace

void BlobDeleter::operator()(uint8_t* data) const { heap_caps_free(data); }

bool begin() {
  mounted = SdMan.begin();
  if (!mounted) {
    Serial.println("No usable SD card.");
    return false;
  }
  if (!SdMan.ensureDirectoryExists(DIRECTORY)) {
    Serial.println("ERROR: could not create the save folder");
    mounted = false;
  }
  return mounted;
}

bool ready() { return mounted; }

bool save(const char* name, uint8_t version, const uint8_t* data, size_t size) {
  if (!mounted || size > MAX_DATA_BYTES) return false;

  char savePath[PATH_BYTES];
  char tempPath[PATH_BYTES];
  makePath(savePath, name, "sav");
  makePath(tempPath, name, "tmp");

  uint8_t header[HEADER_BYTES];
  makeHeader(header, version, data, size);

  SdMan.remove(tempPath);
  if (!writeFile(tempPath, header, HEADER_BYTES, data, size) || !fileMatches(tempPath, header, HEADER_BYTES, data, size)) {
    Serial.printf("ERROR: could not write %s\n", tempPath);
    return false;
  }
  // FAT can't rename over a file, so the old save is removed first. If power fails
  // between the two steps, load() finds the finished temp file.
  if (!SdMan.replaceFile(tempPath, savePath)) {
    Serial.printf("ERROR: could not replace %s\n", savePath);
    return false;
  }
  return true;
}

bool load(const char* name, uint8_t& version, uint8_t* data, size_t capacity, size_t& size) {
  if (!mounted) return false;

  char path[PATH_BYTES];
  makePath(path, name, "sav");
  if (readValid(path, version, data, capacity, size)) return true;
  if (SdMan.exists(path)) Serial.printf("%s is damaged\n", path);

  makePath(path, name, "tmp");
  if (readValid(path, version, data, capacity, size)) {
    Serial.printf("Recovered %s\n", path);
    return true;
  }
  return false;
}

bool erase(const char* name) {
  if (!mounted) return false;

  char path[PATH_BYTES];
  bool erased = true;
  for (const char* extension : {"sav", "tmp"}) {
    makePath(path, name, extension);
    if (SdMan.exists(path) && !SdMan.remove(path)) erased = false;
  }
  return erased;
}

bool saveBlob(const char* name, const uint8_t* data, size_t size) {
  if (!mounted) return false;

  char blobPath[PATH_BYTES];
  char tempPath[PATH_BYTES];
  makePath(blobPath, name, "bin");
  makePath(tempPath, name, "bit");

  uint8_t header[BLOB_HEADER_BYTES] = {'B', 'L', BLOB_FORMAT};
  writeUint32(header + 3, static_cast<uint32_t>(size));
  writeUint32(header + 7, ~updateChecksum(0xFFFFFFFFu, data, size));

  SdMan.remove(tempPath);
  if (!writeFile(tempPath, header, BLOB_HEADER_BYTES, data, size) || !fileMatches(tempPath, header, BLOB_HEADER_BYTES, data, size)) {
    Serial.printf("ERROR: could not write %s\n", tempPath);
    return false;
  }
  if (!SdMan.replaceFile(tempPath, blobPath)) {
    Serial.printf("ERROR: could not replace %s\n", blobPath);
    return false;
  }
  return true;
}

Blob loadBlob(const char* name, size_t& size) {
  if (!mounted) return nullptr;

  char path[PATH_BYTES];
  makePath(path, name, "bin");
  Blob data = readValidBlob(path, size);
  if (data) return data;
  if (SdMan.exists(path)) Serial.printf("%s is damaged\n", path);

  makePath(path, name, "bit");
  data = readValidBlob(path, size);
  if (data) Serial.printf("Recovered %s\n", path);
  return data;
}

}  // namespace storage

#include "pocketbgg_image.h"

#include <ImageToneMapper.h>

#include <climits>
#include <cstdlib>
#include <utility>

#include "third_party/stb_image.h"

namespace pocketbgg {

namespace {

// A JPEG's frame header says how it's coded: SOF0 and SOF1 baseline, SOF2 progressive. The
// markers before it each give their length, so the walk skips from one to the next.
ImageFormat jpegKind(const uint8_t* data, size_t size) {
  size_t at = 2;
  while (at + 4 <= size) {
    if (data[at] != 0xFF) return ImageFormat::Jpeg;
    const uint8_t marker = data[at + 1];
    if (marker == 0xFF) {  // fill bytes before a marker
      at++;
      continue;
    }
    if (marker == 0xC0 || marker == 0xC1) return ImageFormat::Jpeg;
    if (marker == 0xC2) return ImageFormat::ProgressiveJpeg;
    if (marker == 0xDA || marker == 0xD9) return ImageFormat::Jpeg;  // the data, or the end
    const size_t length = (static_cast<size_t>(data[at + 2]) << 8) | data[at + 3];
    if (length < 2) return ImageFormat::Jpeg;
    at += 2 + length;
  }
  return ImageFormat::Jpeg;
}

// The tone mapper hands over each finished row as levels, 0 black and 1 white, which become
// bits here.
struct Packer {
  Picture* picture;
  int rowBytes;
};

void packRow(void* context, int y, const uint8_t* levels, int width) {
  Packer& packer = *static_cast<Packer*>(context);
  uint8_t* row = packer.picture->bits.data() + static_cast<size_t>(y) * packer.rowBytes;
  for (int x = 0; x < width; x++) {
    const uint8_t mask = 0x80 >> (x % 8);
    if (levels[x] == 0) {
      row[x / 8] &= static_cast<uint8_t>(~mask);
    } else {
      row[x / 8] |= mask;
    }
  }
}

}  // namespace

ImageFormat imageFormat(const uint8_t* data, size_t size) {
  static const uint8_t PNG[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  if (size >= sizeof(PNG)) {
    bool png = true;
    for (size_t i = 0; i < sizeof(PNG); i++) png = png && data[i] == PNG[i];
    if (png) return ImageFormat::Png;
  }
  if (size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) return jpegKind(data, size);
  return ImageFormat::Unknown;
}

const char* formatName(ImageFormat format) {
  switch (format) {
    case ImageFormat::Jpeg:
      return "JPEG";
    case ImageFormat::ProgressiveJpeg:
      return "progressive JPEG";
    case ImageFormat::Png:
      return "PNG";
    case ImageFormat::Unknown:
      break;
  }
  return "not an image";
}

const char* pictureErrorText(PictureResult result) {
  switch (result) {
    case PictureResult::Ok:
      return "ok";
    case PictureResult::NotImage:
      return "not a JPEG or PNG";
    case PictureResult::TooLarge:
      return "too large";
    case PictureResult::Unreadable:
      return "damaged or unsupported";
    case PictureResult::NoMemory:
      return "no memory";
  }
  return "?";
}

void fitInside(int width, int height, int boxWidth, int boxHeight, int& outWidth, int& outHeight) {
  outWidth = outHeight = 0;
  if (width <= 0 || height <= 0 || boxWidth <= 0 || boxHeight <= 0) return;
  // Wider than the box's shape: the width decides. Otherwise the height does.
  if (static_cast<long long>(width) * boxHeight >= static_cast<long long>(height) * boxWidth) {
    outWidth = boxWidth < 2 * width ? boxWidth : 2 * width;
    outHeight = static_cast<int>((static_cast<long long>(height) * outWidth + width / 2) / width);
  } else {
    outHeight = boxHeight < 2 * height ? boxHeight : 2 * height;
    outWidth = static_cast<int>((static_cast<long long>(width) * outHeight + height / 2) / height);
  }
  if (outWidth < 1) outWidth = 1;
  if (outHeight < 1) outHeight = 1;
}

PictureResult makePicture(const uint8_t* data, size_t size, int boxWidth, int boxHeight, Picture& out) {
  out = Picture();
  if (data == nullptr || imageFormat(data, size) == ImageFormat::Unknown || size > INT_MAX) {
    return PictureResult::NotImage;
  }
  const int length = static_cast<int>(size);

  // The header first, so a huge image is turned away before anything is allocated for it.
  int width = 0, height = 0, channels = 0;
  if (!stbi_info_from_memory(data, length, &width, &height, &channels)) return PictureResult::Unreadable;
  if (width <= 0 || height <= 0 || static_cast<long long>(width) * height > kMaxSourcePixels) {
    return PictureResult::TooLarge;
  }

  // Grey and alpha, two bytes a pixel, whatever the file holds; a JPEG's alpha is all 255.
  int gotWidth = 0, gotHeight = 0, gotChannels = 0;
  uint8_t* pixels = stbi_load_from_memory(data, length, &gotWidth, &gotHeight, &gotChannels, 2);
  if (pixels == nullptr) return PictureResult::Unreadable;
  if (gotWidth != width || gotHeight != height) {
    stbi_image_free(pixels);
    return PictureResult::Unreadable;
  }

  int outWidth = 0, outHeight = 0;
  fitInside(width, height, boxWidth, boxHeight, outWidth, outHeight);
  const int rowBytes = (outWidth + 7) / 8;
  Picture picture;
  picture.width = outWidth;
  picture.height = outHeight;
  picture.bits.assign(static_cast<size_t>(rowBytes) * outHeight, 0xFF);  // the padding stays white

  // The SDK's tone mapper averages the source pixels that land on each screen pixel, then
  // dithers with Floyd-Steinberg: each pixel's rounding error is passed on to the ones right
  // and below, so a grey area becomes a mix of black and white dots in the right proportion.
  Packer packer{&picture, rowBytes};
  freeink::ImageToneMapper mapper;
  uint8_t* grey = static_cast<uint8_t*>(std::malloc(width));
  if (grey == nullptr || !mapper.begin(width, height, outWidth, outHeight, 2, packRow, &packer)) {
    std::free(grey);
    stbi_image_free(pixels);
    return PictureResult::NoMemory;
  }
  for (int y = 0; y < height; y++) {
    const uint8_t* source = pixels + static_cast<size_t>(y) * width * 2;
    for (int x = 0; x < width; x++) {
      // Laid on white: fully transparent is white, fully opaque is the pixel's own grey.
      const int value = source[2 * x];
      const int alpha = source[2 * x + 1];
      grey[x] = static_cast<uint8_t>((value * alpha + 255 * (255 - alpha) + 127) / 255);
    }
    mapper.addSourceRow(grey);
  }
  mapper.finish();
  std::free(grey);
  stbi_image_free(pixels);

  out = std::move(picture);
  return PictureResult::Ok;
}

}  // namespace pocketbgg

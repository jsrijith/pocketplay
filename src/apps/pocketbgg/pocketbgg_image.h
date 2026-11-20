#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// A box-art thumbnail turned into a 1-bit picture for the panel: decoded from JPEG or PNG,
// fitted into a box, and dithered to black and white. Nothing here touches the card or the
// screen, so it runs in the PC tests too.
namespace pocketbgg {

// Rows padded to whole bytes, most significant bit first, 0 = black: drawIcon()'s format,
// which is the panel's own, so drawing it is copying bits.
struct Picture {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> bits;
  bool empty() const { return bits.empty(); }
};

enum class ImageFormat { Unknown, Jpeg, ProgressiveJpeg, Png };
ImageFormat imageFormat(const uint8_t* data, size_t size);
const char* formatName(ImageFormat format);

enum class PictureResult { Ok, NotImage, TooLarge, Unreadable, NoMemory };
const char* pictureErrorText(PictureResult result);

// A progressive JPEG of this many pixels needs about 3 bytes each while decoding, about
// 0.8 MB here. BGG's thumbnails are up to 200 x 200.
constexpr int kMaxSourcePixels = 512 * 512;

// The size that fits inside the box with the picture's shape kept, never more than twice as
// large as it was.
void fitInside(int width, int height, int boxWidth, int boxHeight, int& outWidth, int& outHeight);

// Decodes `data`, fits it into the box and dithers it. Transparent pixels come out white.
PictureResult makePicture(const uint8_t* data, size_t size, int boxWidth, int boxHeight, Picture& out);

}  // namespace pocketbgg

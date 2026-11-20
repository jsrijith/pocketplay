// PocketBGG's thumbnails: decoding, fitting and dithering, on images made from plain shapes by
// make_images.py (a progressive and a baseline JPEG, a PNG with transparency, one too large),
// and on damaged files, which must fail without a crash.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "pocketbgg_image.h"

using namespace pocketbgg;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

std::string readFile(const char* name) {
  std::ifstream file(name, std::ios::binary);
  std::stringstream bytes;
  bytes << file.rdbuf();
  return bytes.str();
}

const uint8_t* bytesOf(const std::string& s) { return reinterpret_cast<const uint8_t*>(s.data()); }

bool white(const Picture& p, int x, int y) {
  const int rowBytes = (p.width + 7) / 8;
  return (p.bits[static_cast<size_t>(y) * rowBytes + x / 8] & (0x80 >> (x % 8))) != 0;
}

// The share of white pixels in a rectangle, 0 to 1.
double whiteShare(const Picture& p, int left, int top, int width, int height) {
  int count = 0;
  for (int y = top; y < top + height; y++) {
    for (int x = left; x < left + width; x++) count += white(p, x, y) ? 1 : 0;
  }
  return static_cast<double>(count) / (width * height);
}

void testFit() {
  int w = 0, h = 0;
  fitInside(200, 150, 200, 200, w, h);
  check(w == 200 && h == 150, "a thumbnail that fits keeps its size");
  fitInside(400, 300, 200, 200, w, h);
  check(w == 200 && h == 150, "a wide image shrinks to the box's width");
  fitInside(150, 300, 200, 200, w, h);
  check(w == 100 && h == 200, "a tall image shrinks to the box's height");
  fitInside(50, 50, 200, 200, w, h);
  check(w == 100 && h == 100, "a small image grows to twice its size, no more");
  fitInside(2000, 1, 200, 200, w, h);
  check(w == 200 && h == 1, "a thin strip keeps at least one row");
  fitInside(0, 10, 200, 200, w, h);
  check(w == 0 && h == 0, "no size for an empty image");
}

void testFormats() {
  const std::string progressive = readFile("image_progressive.jpg");
  const std::string baseline = readFile("image_baseline.jpg");
  const std::string png = readFile("image_alpha.png");
  check(!progressive.empty() && !baseline.empty() && !png.empty(), "the test images are here");
  check(imageFormat(bytesOf(progressive), progressive.size()) == ImageFormat::ProgressiveJpeg,
        "a progressive JPEG is seen as one");
  check(imageFormat(bytesOf(baseline), baseline.size()) == ImageFormat::Jpeg, "a baseline JPEG is seen as one");
  check(imageFormat(bytesOf(png), png.size()) == ImageFormat::Png, "a PNG is seen as one");
  const std::string text = "<html>";
  check(imageFormat(bytesOf(text), text.size()) == ImageFormat::Unknown, "text isn't an image");
  check(imageFormat(bytesOf(png), 3) == ImageFormat::Unknown, "three bytes of a PNG aren't one");
}

// Black, grey and white thirds: the black third all black, the white all white, and the grey
// about half and half, dithered.
void checkThirds(const std::string& file, const char* what) {
  Picture p;
  const PictureResult result = makePicture(bytesOf(file), file.size(), 200, 200, p);
  check(result == PictureResult::Ok, what);
  if (result != PictureResult::Ok) return;
  check(p.width == 200 && p.height == 150, "the picture keeps the thumbnail's size");
  check(p.bits.size() == 25u * 150u, "25 bytes a row, 150 rows");
  check(whiteShare(p, 4, 4, 56, 142) == 0.0, "the black third is black");
  check(whiteShare(p, 140, 4, 56, 142) == 1.0, "the white third is white");
  const double grey = whiteShare(p, 72, 4, 54, 142);
  check(grey > 0.4 && grey < 0.6, "the grey third is about half white");
}

void testJpegs() {
  checkThirds(readFile("image_progressive.jpg"), "a progressive JPEG decodes");
  checkThirds(readFile("image_baseline.jpg"), "a baseline JPEG decodes");
}

void testPng() {
  const std::string png = readFile("image_alpha.png");
  Picture p;
  check(makePicture(bytesOf(png), png.size(), 200, 200, p) == PictureResult::Ok, "a PNG with alpha decodes");
  check(p.width == 200 && p.height == 200, "a 100 px PNG grows to 200");
  if (p.width != 200 || p.height != 200) return;
  check(whiteShare(p, 0, 0, 200, 96) == 1.0, "transparent pixels are white, not what's under them");
  check(whiteShare(p, 0, 104, 200, 96) == 0.0, "opaque black is black");
}

void testRefused() {
  Picture p;
  const std::string large = readFile("image_large.png");
  check(makePicture(bytesOf(large), large.size(), 200, 200, p) == PictureResult::TooLarge, "1000 x 1000 is too large");
  check(p.empty(), "nothing is kept from a refused image");

  const std::string text = "not an image at all";
  check(makePicture(bytesOf(text), text.size(), 200, 200, p) == PictureResult::NotImage, "text is refused");
  check(makePicture(nullptr, 0, 200, 200, p) == PictureResult::NotImage, "no bytes are refused");

  // A JPEG's start followed by rubbish.
  std::string rubbish = "\xFF\xD8\xFF\xE0";
  for (int i = 0; i < 500; i++) rubbish += static_cast<char>((i * 37) & 0xFF);
  check(makePicture(bytesOf(rubbish), rubbish.size(), 200, 200, p) != PictureResult::Ok, "a damaged JPEG fails");

  // Files cut short at every few bytes: each must either fail or give a whole picture, and
  // never read past what it was given (ASan watches that).
  const char* names[] = {"image_progressive.jpg", "image_baseline.jpg", "image_alpha.png"};
  for (const char* name : names) {
    const std::string whole = readFile(name);
    for (size_t cut = 0; cut < whole.size(); cut += 7) {
      const std::string part = whole.substr(0, cut);
      Picture q;
      const PictureResult result = makePicture(bytesOf(part), part.size(), 200, 200, q);
      if (result == PictureResult::Ok) {
        check(q.bits.size() == static_cast<size_t>((q.width + 7) / 8) * q.height, "a cut file gives a whole picture");
      } else {
        check(q.empty(), "a cut file that fails keeps nothing");
      }
    }
  }
}

}  // namespace

int main() {
  testFit();
  testFormats();
  testJpegs();
  testPng();
  testRefused();
  if (failures) {
    std::printf("pocketbgg image: %d failed\n", failures);
    return 1;
  }
  std::printf("pocketbgg image: all passed\n");
  return 0;
}

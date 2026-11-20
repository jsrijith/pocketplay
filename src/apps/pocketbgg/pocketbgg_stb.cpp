// stb_image's code, built once, for JPEG and PNG from memory only.
//
// It's here rather than tjpgd because BGG's JPEG thumbnails are progressive: the whole picture
// comes coarse first and is sharpened in later passes, so a decoder has to keep every block's
// coefficients until the last pass. tjpgd decodes block by block and can't; stb_image keeps
// them, in PSRAM, since malloc puts anything over 4 KB there.

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO          // only bytes already in memory, never a file
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_THREAD_LOCALS  // one task decodes, loop()
#define STBI_MAX_DIMENSIONS 2048

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include "third_party/stb_image.h"
#pragma GCC diagnostic pop

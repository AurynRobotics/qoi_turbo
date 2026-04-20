#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace qoi_turbo {

static constexpr uint32_t MAGIC = 0x716F6966;  // "qoif"
static constexpr int HEADER_SIZE = 14;
static constexpr int PIXELS_MAX = 400000000;

static constexpr uint8_t OP_INDEX = 0x00;  // 00xxxxxx
static constexpr uint8_t OP_DIFF = 0x40;   // 01xxxxxx
static constexpr uint8_t OP_LUMA = 0x80;   // 10xxxxxx
static constexpr uint8_t OP_RUN = 0xC0;    // 11xxxxxx
static constexpr uint8_t OP_RGB = 0xFE;    // 11111110
static constexpr uint8_t OP_RGBA = 0xFF;   // 11111111
static constexpr uint8_t MASK_2 = 0xC0;    // 11000000

static constexpr uint8_t PADDING[8] = {0, 0, 0, 0, 0, 0, 0, 1};

union rgba_t {
  struct {
    uint8_t r, g, b, a;
  } rgba;
  uint32_t v;
};

struct desc_t {
  uint32_t width;
  uint32_t height;
  uint8_t channels;
  uint8_t colorspace;
};

// Original hash for reference compatibility verification
inline uint8_t pixel_hash(rgba_t px) {
  return (px.rgba.r * 3 + px.rgba.g * 5 + px.rgba.b * 7 + px.rgba.a * 11) % 64;
}

// Blend2D-style fast hash: same result, fewer instructions
inline uint8_t pixel_hash_fast(uint32_t v) {
  uint64_t w = (static_cast<uint64_t>(v) << 32) | v;
  w &= 0xFF00FF0000FF00FFull;
  // Multiplier places r*3, b*7, g*5, a*11 in the right positions
  // For little-endian RGBA layout: byte0=R, byte1=G, byte2=B, byte3=A
  return static_cast<uint8_t>((w * ((3ull << 56) | (7ull << 40) | (5ull << 16) | 11ull)) >> 56) & 63;
}

inline void write_32(uint8_t* bytes, int& p, uint32_t v) {
  bytes[p++] = (v >> 24) & 0xFF;
  bytes[p++] = (v >> 16) & 0xFF;
  bytes[p++] = (v >> 8) & 0xFF;
  bytes[p++] = v & 0xFF;
}

inline uint32_t read_32(const uint8_t* bytes, int& p) {
  uint32_t a = bytes[p++];
  uint32_t b = bytes[p++];
  uint32_t c = bytes[p++];
  uint32_t d = bytes[p++];
  return (a << 24) | (b << 16) | (c << 8) | d;
}

}  // namespace qoi_turbo

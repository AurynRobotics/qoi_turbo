#pragma once

#include <cstring>

#include "qoi_turbo_common.hpp"

namespace qoi_turbo {

inline int decode_header(const uint8_t* data, int size, desc_t& desc) {
  if (size < HEADER_SIZE + 8) {
    return 0;
  }

  int p = 0;
  uint32_t magic = read_32(data, p);
  desc.width = read_32(data, p);
  desc.height = read_32(data, p);
  desc.channels = data[p++];
  desc.colorspace = data[p++];

  if (magic != MAGIC || desc.width == 0 || desc.height == 0 || desc.channels < 3 || desc.channels > 4 ||
      desc.colorspace > 1 || desc.height >= PIXELS_MAX / desc.width) {
    return 0;
  }
  return desc.width * desc.height * desc.channels;
}

inline int decode_to(const uint8_t* __restrict data, int size, const desc_t& desc, uint8_t* __restrict out) {
  const int channels = desc.channels;
  const int px_len = desc.width * desc.height * channels;
  int p = HEADER_SIZE;

  alignas(64) rgba_t index[64] = {};

  rgba_t px;
  px.v = 0;
  px.rgba.a = 255;

  const int chunks_len = size - 8;

  if (channels == 4) {
    // Opcode-driven loop: no run counter, inline RUN batch-fill
    int px_pos = 0;
    while (px_pos < px_len && p < chunks_len) {
      __builtin_prefetch(data + p + 64, 0, 1);
      const unsigned b1 = data[p++];

      if (__builtin_expect(b1 < 0xC0, 1)) {
        if (b1 < 0x40) {
          // OP_INDEX
          px = index[b1];
        } else if (__builtin_expect(b1 < 0x80, 1)) {
          // OP_DIFF
          px.rgba.r += ((b1 >> 4) & 0x03) - 2;
          px.rgba.g += ((b1 >> 2) & 0x03) - 2;
          px.rgba.b += (b1 & 0x03) - 2;
          index[pixel_hash_fast(px.v)] = px;
        } else {
          // OP_LUMA
          const unsigned b2 = data[p++];
          int vg = (b1 & 0x3f) - 32;
          px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
          px.rgba.g += vg;
          px.rgba.b += vg - 8 + (b2 & 0x0f);
          index[pixel_hash_fast(px.v)] = px;
        }
        std::memcpy(out + px_pos, &px.v, 4);
        px_pos += 4;
      } else if (__builtin_expect(b1 < 0xFE, 1)) {
        // OP_RUN: batch-fill and advance
        int run = (b1 & 0x3f) + 1;
        uint32_t* dst = reinterpret_cast<uint32_t*>(out + px_pos);
        for (int i = 0; i < run; i++) {
          dst[i] = px.v;
        }
        px_pos += run * 4;
      } else if (b1 == 0xFE) {
        px.rgba.r = data[p++];
        px.rgba.g = data[p++];
        px.rgba.b = data[p++];
        index[pixel_hash_fast(px.v)] = px;
        std::memcpy(out + px_pos, &px.v, 4);
        px_pos += 4;
      } else {
        px.rgba.r = data[p++];
        px.rgba.g = data[p++];
        px.rgba.b = data[p++];
        px.rgba.a = data[p++];
        index[pixel_hash_fast(px.v)] = px;
        std::memcpy(out + px_pos, &px.v, 4);
        px_pos += 4;
      }
    }
    if (px_pos != px_len) {
      return 0;
    }
  } else {
    int px_pos = 0;
    while (px_pos < px_len && p < chunks_len) {
      const unsigned b1 = data[p++];

      if (__builtin_expect(b1 < 0xC0, 1)) {
        if (b1 < 0x40) {
          px = index[b1];
        } else if (__builtin_expect(b1 < 0x80, 1)) {
          px.rgba.r += ((b1 >> 4) & 0x03) - 2;
          px.rgba.g += ((b1 >> 2) & 0x03) - 2;
          px.rgba.b += (b1 & 0x03) - 2;
          index[pixel_hash_fast(px.v)] = px;
        } else {
          const unsigned b2 = data[p++];
          int vg = (b1 & 0x3f) - 32;
          px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
          px.rgba.g += vg;
          px.rgba.b += vg - 8 + (b2 & 0x0f);
          index[pixel_hash_fast(px.v)] = px;
        }
        out[px_pos + 0] = px.rgba.r;
        out[px_pos + 1] = px.rgba.g;
        out[px_pos + 2] = px.rgba.b;
        px_pos += 3;
      } else if (__builtin_expect(b1 < 0xFE, 1)) {
        int run = (b1 & 0x3f) + 1;
        for (int i = 0; i < run; i++) {
          out[px_pos + 0] = px.rgba.r;
          out[px_pos + 1] = px.rgba.g;
          out[px_pos + 2] = px.rgba.b;
          px_pos += 3;
        }
      } else if (b1 == 0xFE) {
        px.rgba.r = data[p++];
        px.rgba.g = data[p++];
        px.rgba.b = data[p++];
        index[pixel_hash_fast(px.v)] = px;
        out[px_pos + 0] = px.rgba.r;
        out[px_pos + 1] = px.rgba.g;
        out[px_pos + 2] = px.rgba.b;
        px_pos += 3;
      } else {
        px.rgba.r = data[p++];
        px.rgba.g = data[p++];
        px.rgba.b = data[p++];
        px.rgba.a = data[p++];
        index[pixel_hash_fast(px.v)] = px;
        out[px_pos + 0] = px.rgba.r;
        out[px_pos + 1] = px.rgba.g;
        out[px_pos + 2] = px.rgba.b;
        px_pos += 3;
      }
    }
    if (px_pos != px_len) {
      return 0;
    }
  }

  return px_len;
}

inline std::vector<uint8_t> decode(const uint8_t* data, int size, desc_t& desc) {
  int px_len = decode_header(data, size, desc);
  if (px_len == 0) {
    return {};
  }

  std::vector<uint8_t> pixels(px_len);
  decode_to(data, size, desc, pixels.data());
  return pixels;
}

}  // namespace qoi_turbo

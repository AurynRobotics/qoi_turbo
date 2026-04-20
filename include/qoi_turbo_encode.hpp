#pragma once

#include <cstdlib>

#include "qoi_turbo_common.hpp"

namespace qoi_turbo {

inline int encode_max_size(const desc_t& desc) {
  return desc.width * desc.height * (desc.channels + 1) + HEADER_SIZE + 8;
}

// Detect if any pixel has alpha != 255 by sampling the image.
// For 4-channel RGBA data, checks every pixel's alpha byte.
inline bool has_alpha_channel(const uint8_t* pixels, int total_pixels) {
  // Check all pixels — the alpha byte is at offset 3 of each 4-byte pixel.
  // Use uint32_t scan: if all alphas are 255, then (v | 0x00FFFFFF) == 0xFFFFFFFF
  // for every pixel (on little-endian: alpha is the high byte).
  const uint32_t* p = reinterpret_cast<const uint32_t*>(pixels);
  for (int i = 0; i < total_pixels; i++) {
    rgba_t px;
    px.v = p[i];
    if (px.rgba.a != 255) {
      return true;
    }
  }
  return false;
}

// Templated 4-channel encoder: has_alpha=false skips alpha branch + RGBA opcode
template <bool has_alpha>
static inline int encode_4ch(const uint8_t* __restrict pixels, const desc_t& desc, uint8_t* __restrict out) {
  const int px_len = desc.width * desc.height * 4;
  const int px_end = px_len - 4;
  int p = 0;

  write_32(out, p, MAGIC);
  write_32(out, p, desc.width);
  write_32(out, p, desc.height);
  out[p++] = desc.channels;
  out[p++] = desc.colorspace;

  alignas(64) rgba_t index[64] = {};
  int run = 0;
  rgba_t px_prev;
  px_prev.v = 0;
  px_prev.rgba.a = 255;
  rgba_t px = px_prev;

#define QOI_TURBO_ENCODE_4CH(PX_POS)                               \
  do {                                                             \
    std::memcpy(&px.v, pixels + (PX_POS), 4);                      \
    if (px.v == px_prev.v) {                                       \
      run++;                                                       \
      if (__builtin_expect(run == 62, 0) | ((PX_POS) == px_end)) { \
        out[p++] = OP_RUN | (run - 1);                             \
        run = 0;                                                   \
      }                                                            \
    } else {                                                       \
      if (run > 0) {                                               \
        out[p++] = OP_RUN | (run - 1);                             \
        run = 0;                                                   \
      }                                                            \
      const uint8_t idx = pixel_hash_fast(px.v);                   \
      if (index[idx].v == px.v) {                                  \
        out[p++] = OP_INDEX | idx;                                 \
      } else {                                                     \
        index[idx] = px;                                           \
        if constexpr (has_alpha) {                                 \
          if (__builtin_expect(px.rgba.a == px_prev.rgba.a, 1)) {  \
            int vr = (int8_t)(px.rgba.r - px_prev.rgba.r);         \
            int vg = (int8_t)(px.rgba.g - px_prev.rgba.g);         \
            int vb = (int8_t)(px.rgba.b - px_prev.rgba.b);         \
            unsigned dr2 = (unsigned)(vr + 2);                     \
            unsigned dg2 = (unsigned)(vg + 2);                     \
            unsigned db2 = (unsigned)(vb + 2);                     \
            int vg_r = vr - vg, vg_b = vb - vg;                    \
            unsigned ug = (unsigned)(vg + 32);                     \
            unsigned ug_r = (unsigned)(vg_r + 8);                  \
            unsigned ug_b = (unsigned)(vg_b + 8);                  \
            if ((dr2 | dg2 | db2) < 4u) {                          \
              out[p++] = OP_DIFF | (dr2 << 4) | (dg2 << 2) | db2;  \
            } else if (ug < 64u && (ug_r | ug_b) < 16u) {          \
              out[p++] = OP_LUMA | ug;                             \
              out[p++] = (ug_r << 4) | ug_b;                       \
            } else {                                               \
              out[p++] = OP_RGB;                                   \
              out[p++] = px.rgba.r;                                \
              out[p++] = px.rgba.g;                                \
              out[p++] = px.rgba.b;                                \
            }                                                      \
          } else {                                                 \
            out[p++] = OP_RGBA;                                    \
            out[p++] = px.rgba.r;                                  \
            out[p++] = px.rgba.g;                                  \
            out[p++] = px.rgba.b;                                  \
            out[p++] = px.rgba.a;                                  \
          }                                                        \
        } else {                                                   \
          /* No alpha: skip alpha check, never emit RGBA */        \
          int vr = (int8_t)(px.rgba.r - px_prev.rgba.r);           \
          int vg = (int8_t)(px.rgba.g - px_prev.rgba.g);           \
          int vb = (int8_t)(px.rgba.b - px_prev.rgba.b);           \
          unsigned dr2 = (unsigned)(vr + 2);                       \
          unsigned dg2 = (unsigned)(vg + 2);                       \
          unsigned db2 = (unsigned)(vb + 2);                       \
          int vg_r = vr - vg, vg_b = vb - vg;                      \
          unsigned ug = (unsigned)(vg + 32);                       \
          unsigned ug_r = (unsigned)(vg_r + 8);                    \
          unsigned ug_b = (unsigned)(vg_b + 8);                    \
          if ((dr2 | dg2 | db2) < 4u) {                            \
            out[p++] = OP_DIFF | (dr2 << 4) | (dg2 << 2) | db2;    \
          } else if (ug < 64u && (ug_r | ug_b) < 16u) {            \
            out[p++] = OP_LUMA | ug;                               \
            out[p++] = (ug_r << 4) | ug_b;                         \
          } else {                                                 \
            out[p++] = OP_RGB;                                     \
            out[p++] = px.rgba.r;                                  \
            out[p++] = px.rgba.g;                                  \
            out[p++] = px.rgba.b;                                  \
          }                                                        \
        }                                                          \
      }                                                            \
    }                                                              \
    px_prev = px;                                                  \
  } while (0)

  int px_pos = 0;
  for (; px_pos + 8 <= px_len; px_pos += 8) {
    QOI_TURBO_ENCODE_4CH(px_pos);
    QOI_TURBO_ENCODE_4CH(px_pos + 4);
  }
  for (; px_pos < px_len; px_pos += 4) {
    QOI_TURBO_ENCODE_4CH(px_pos);
  }

#undef QOI_TURBO_ENCODE_4CH

  for (int i = 0; i < 8; i++) {
    out[p++] = PADDING[i];
  }

  return p;
}

inline int encode_to(const uint8_t* __restrict pixels, const desc_t& desc, uint8_t* __restrict out) {
  const int channels = desc.channels;
  const int total_pixels = desc.width * desc.height;

  if (channels == 4) {
    // Auto-detect: scan for non-255 alpha to pick the fast path
    if (has_alpha_channel(pixels, total_pixels)) {
      return encode_4ch<true>(pixels, desc, out);
    } else {
      return encode_4ch<false>(pixels, desc, out);
    }
  }

  // 3-channel path (unchanged)
  const int px_len = total_pixels * channels;
  const int px_end = px_len - channels;
  int p = 0;

  write_32(out, p, MAGIC);
  write_32(out, p, desc.width);
  write_32(out, p, desc.height);
  out[p++] = desc.channels;
  out[p++] = desc.colorspace;

  alignas(64) rgba_t index[64] = {};
  int run = 0;
  rgba_t px_prev;
  px_prev.v = 0;
  px_prev.rgba.a = 255;
  rgba_t px = px_prev;

  for (int px_pos = 0; px_pos < px_len; px_pos += 3) {
    px.rgba.r = pixels[px_pos + 0];
    px.rgba.g = pixels[px_pos + 1];
    px.rgba.b = pixels[px_pos + 2];

    if (px.v == px_prev.v) {
      run++;
      if (__builtin_expect(run == 62, 0) | (px_pos == px_end)) {
        out[p++] = OP_RUN | (run - 1);
        run = 0;
      }
    } else {
      if (run > 0) {
        out[p++] = OP_RUN | (run - 1);
        run = 0;
      }

      const uint8_t idx = pixel_hash_fast(px.v);

      if (index[idx].v == px.v) {
        out[p++] = OP_INDEX | idx;
      } else {
        index[idx] = px;

        int vr = (int8_t)(px.rgba.r - px_prev.rgba.r);
        int vg = (int8_t)(px.rgba.g - px_prev.rgba.g);
        int vb = (int8_t)(px.rgba.b - px_prev.rgba.b);

        unsigned dr2 = (unsigned)(vr + 2);
        unsigned dg2 = (unsigned)(vg + 2);
        unsigned db2 = (unsigned)(vb + 2);

        if ((dr2 | dg2 | db2) < 4u) {
          out[p++] = OP_DIFF | (dr2 << 4) | (dg2 << 2) | db2;
        } else {
          int vg_r = vr - vg;
          int vg_b = vb - vg;
          unsigned ug = (unsigned)(vg + 32);
          unsigned ug_r = (unsigned)(vg_r + 8);
          unsigned ug_b = (unsigned)(vg_b + 8);

          if (ug < 64u && (ug_r | ug_b) < 16u) {
            out[p++] = OP_LUMA | ug;
            out[p++] = (ug_r << 4) | ug_b;
          } else {
            out[p++] = OP_RGB;
            out[p++] = px.rgba.r;
            out[p++] = px.rgba.g;
            out[p++] = px.rgba.b;
          }
        }
      }
    }
    px_prev = px;
  }

  for (int i = 0; i < 8; i++) {
    out[p++] = PADDING[i];
  }

  return p;
}

inline std::vector<uint8_t> encode(const uint8_t* pixels, const desc_t& desc) {
  int max_size = encode_max_size(desc);
  uint8_t* buf = static_cast<uint8_t*>(std::malloc(max_size));
  if (!buf) {
    return {};
  }

  int len = encode_to(pixels, desc, buf);
  std::vector<uint8_t> result(buf, buf + len);
  std::free(buf);
  return result;
}

}  // namespace qoi_turbo

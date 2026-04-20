#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define QOI_IMPLEMENTATION
#include "qoi.h"
#include <qoi_turbo.hpp>

// ---- Test image generators ----

static std::vector<uint8_t> make_gradient(int w, int h, int ch) {
  std::vector<uint8_t> img(w * h * ch);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int i = (y * w + x) * ch;
      img[i + 0] = (x * 255) / (w - 1 > 0 ? w - 1 : 1);
      img[i + 1] = (y * 255) / (h - 1 > 0 ? h - 1 : 1);
      img[i + 2] = ((x + y) * 127) / (w + h - 2 > 0 ? w + h - 2 : 1);
      if (ch == 4) {
        img[i + 3] = 255;
      }
    }
  }
  return img;
}

static std::vector<uint8_t> make_solid(int w, int h, int ch) {
  std::vector<uint8_t> img(w * h * ch);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int i = (y * w + x) * ch;
      int block = (x < w / 2 ? 0 : 1) + (y < h / 2 ? 0 : 2);
      uint8_t colors[4][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}};
      img[i + 0] = colors[block][0];
      img[i + 1] = colors[block][1];
      img[i + 2] = colors[block][2];
      if (ch == 4) {
        img[i + 3] = 255;
      }
    }
  }
  return img;
}

static std::vector<uint8_t> make_noise(int w, int h, int ch) {
  std::vector<uint8_t> img(w * h * ch);
  uint32_t seed = 42;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int i = (y * w + x) * ch;
      seed = seed * 1103515245u + 12345u;
      img[i + 0] = (seed >> 16) & 0xFF;
      seed = seed * 1103515245u + 12345u;
      img[i + 1] = (seed >> 16) & 0xFF;
      seed = seed * 1103515245u + 12345u;
      img[i + 2] = (seed >> 16) & 0xFF;
      if (ch == 4) {
        seed = seed * 1103515245u + 12345u;
        img[i + 3] = (seed >> 16) & 0xFF;
      }
    }
  }
  return img;
}

static std::vector<uint8_t> make_single_pixel(int ch) {
  std::vector<uint8_t> img(ch);
  img[0] = 128;
  img[1] = 64;
  img[2] = 32;
  if (ch == 4) {
    img[3] = 200;
  }
  return img;
}

static std::vector<uint8_t> make_all_same(int w, int h, int ch) {
  std::vector<uint8_t> img(w * h * ch);
  for (int i = 0; i < w * h; i++) {
    img[i * ch + 0] = 100;
    img[i * ch + 1] = 150;
    img[i * ch + 2] = 200;
    if (ch == 4) {
      img[i * ch + 3] = 255;
    }
  }
  return img;
}

static std::vector<uint8_t> make_all_different(int w, int h, int ch) {
  std::vector<uint8_t> img(w * h * ch);
  for (int i = 0; i < w * h; i++) {
    img[i * ch + 0] = (i * 73) & 0xFF;
    img[i * ch + 1] = (i * 137) & 0xFF;
    img[i * ch + 2] = (i * 199) & 0xFF;
    if (ch == 4) {
      img[i * ch + 3] = 255;
    }
  }
  return img;
}

// ---- Test harness ----

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)              \
  do {                          \
    tests_run++;                \
    printf("  %-50s ", (name)); \
    fflush(stdout);             \
  } while (0)

#define PASS()        \
  do {                \
    tests_passed++;   \
    printf("PASS\n"); \
  } while (0)

#define FAIL(msg)              \
  do {                         \
    printf("FAIL: %s\n", msg); \
  } while (0)

// ---- Reference codec wrappers ----

static std::vector<uint8_t> ref_encode(const uint8_t* pixels, int w, int h, int ch) {
  qoi_desc desc;
  desc.width = w;
  desc.height = h;
  desc.channels = ch;
  desc.colorspace = 0;
  int out_len = 0;
  void* encoded = qoi_encode(pixels, &desc, &out_len);
  assert(encoded);
  std::vector<uint8_t> result((uint8_t*)encoded, (uint8_t*)encoded + out_len);
  QOI_FREE(encoded);
  return result;
}

static std::vector<uint8_t> ref_decode(const uint8_t* data, int size, int ch) {
  qoi_desc desc;
  void* decoded = qoi_decode(data, size, &desc, ch);
  assert(decoded);
  int px_len = desc.width * desc.height * ch;
  std::vector<uint8_t> result((uint8_t*)decoded, (uint8_t*)decoded + px_len);
  QOI_FREE(decoded);
  return result;
}

// ---- Tests ----

static void test_bitexact(const char* label, const std::vector<uint8_t>& img, int w, int h, int ch) {
  TEST(label);

  qoi_turbo::desc_t desc;
  desc.width = w;
  desc.height = h;
  desc.channels = ch;
  desc.colorspace = 0;

  auto turbo_enc = qoi_turbo::encode(img.data(), desc);
  auto ref_enc = ref_encode(img.data(), w, h, ch);

  if (turbo_enc.size() != ref_enc.size()) {
    char msg[256];
    snprintf(msg, sizeof(msg), "size mismatch: turbo=%zu ref=%zu", turbo_enc.size(), ref_enc.size());
    FAIL(msg);
    return;
  }
  if (memcmp(turbo_enc.data(), ref_enc.data(), turbo_enc.size()) != 0) {
    for (size_t i = 0; i < turbo_enc.size(); i++) {
      if (turbo_enc[i] != ref_enc[i]) {
        char msg[256];
        snprintf(msg, sizeof(msg), "byte mismatch at %zu: turbo=0x%02x ref=0x%02x", i, turbo_enc[i], ref_enc[i]);
        FAIL(msg);
        return;
      }
    }
  }
  PASS();
}

static void test_roundtrip(const char* label, const std::vector<uint8_t>& img, int w, int h, int ch) {
  TEST(label);

  qoi_turbo::desc_t desc;
  desc.width = w;
  desc.height = h;
  desc.channels = ch;
  desc.colorspace = 0;

  auto encoded = qoi_turbo::encode(img.data(), desc);

  qoi_turbo::desc_t dec_desc;
  auto decoded = qoi_turbo::decode(encoded.data(), (int)encoded.size(), dec_desc);

  if (decoded.size() != img.size()) {
    char msg[256];
    snprintf(msg, sizeof(msg), "decoded size mismatch: got=%zu expected=%zu", decoded.size(), img.size());
    FAIL(msg);
    return;
  }
  if (memcmp(decoded.data(), img.data(), img.size()) != 0) {
    FAIL("decoded pixels differ from original");
    return;
  }
  if (dec_desc.width != desc.width || dec_desc.height != desc.height || dec_desc.channels != desc.channels) {
    FAIL("decoded desc mismatch");
    return;
  }
  PASS();
}

static void test_cross_roundtrip(const char* label, const std::vector<uint8_t>& img, int w, int h, int ch) {
  TEST(label);

  qoi_turbo::desc_t desc;
  desc.width = w;
  desc.height = h;
  desc.channels = ch;
  desc.colorspace = 0;
  auto turbo_enc = qoi_turbo::encode(img.data(), desc);
  auto ref_dec = ref_decode(turbo_enc.data(), (int)turbo_enc.size(), ch);

  if (ref_dec.size() != img.size() || memcmp(ref_dec.data(), img.data(), img.size()) != 0) {
    FAIL("turbo encode -> ref decode mismatch");
    return;
  }

  auto ref_enc = ref_encode(img.data(), w, h, ch);
  qoi_turbo::desc_t dec_desc;
  auto turbo_dec = qoi_turbo::decode(ref_enc.data(), (int)ref_enc.size(), dec_desc);

  if (turbo_dec.size() != img.size() || memcmp(turbo_dec.data(), img.data(), img.size()) != 0) {
    FAIL("ref encode -> turbo decode mismatch");
    return;
  }
  PASS();
}

static int decode_qoi_turbo_truncated(const uint8_t* data, int size) {
  qoi_turbo::desc_t desc;
  desc.width = 64;
  desc.height = 64;
  desc.channels = 4;
  desc.colorspace = 0;
  std::vector<uint8_t> decoded(desc.width * desc.height * 4);
  return qoi_turbo::decode_to(data, size, desc, decoded.data());
}

static void test_truncation(const char* label, const std::vector<uint8_t>& encoded) {
  TEST(label);
  int truncated_size = (int)encoded.size() / 2;
  if (truncated_size <= 0) {
    FAIL("encoded stream too small");
    return;
  }
  if (decode_qoi_turbo_truncated(encoded.data(), truncated_size) != 0) {
    FAIL("truncated stream unexpectedly decoded");
    return;
  }
  PASS();
}

int main() {
  printf("=== qoi_turbo correctness tests ===\n\n");

  printf("Bit-exact encoding tests (turbo output == reference output):\n");
  {
    auto img = make_gradient(320, 240, 4);
    test_bitexact("gradient_320x240_4ch", img, 320, 240, 4);
  }
  {
    auto img = make_gradient(320, 240, 3);
    test_bitexact("gradient_320x240_3ch", img, 320, 240, 3);
  }
  {
    auto img = make_solid(320, 240, 4);
    test_bitexact("solid_320x240_4ch", img, 320, 240, 4);
  }
  {
    auto img = make_noise(320, 240, 4);
    test_bitexact("noise_320x240_4ch", img, 320, 240, 4);
  }
  {
    auto img = make_noise(320, 240, 3);
    test_bitexact("noise_320x240_3ch", img, 320, 240, 3);
  }
  {
    auto img = make_single_pixel(4);
    test_bitexact("single_pixel_4ch", img, 1, 1, 4);
  }
  {
    auto img = make_single_pixel(3);
    test_bitexact("single_pixel_3ch", img, 1, 1, 3);
  }
  {
    auto img = make_all_same(100, 100, 4);
    test_bitexact("all_same_100x100_4ch", img, 100, 100, 4);
  }
  {
    auto img = make_all_same(100, 100, 3);
    test_bitexact("all_same_100x100_3ch", img, 100, 100, 3);
  }
  {
    auto img = make_all_different(100, 100, 4);
    test_bitexact("all_different_100x100_4ch", img, 100, 100, 4);
  }
  {
    auto img = make_gradient(37, 19, 4);
    test_bitexact("gradient_37x19_4ch", img, 37, 19, 4);
  }
  {
    // Run length boundary: 62 identical pixels then different
    std::vector<uint8_t> img(65 * 4);
    for (int i = 0; i < 62; i++) {
      img[i * 4 + 0] = 100;
      img[i * 4 + 1] = 150;
      img[i * 4 + 2] = 200;
      img[i * 4 + 3] = 255;
    }
    for (int i = 62; i < 65; i++) {
      img[i * 4 + 0] = (uint8_t)(i * 37);
      img[i * 4 + 1] = (uint8_t)(i * 73);
      img[i * 4 + 2] = (uint8_t)(i * 111);
      img[i * 4 + 3] = 255;
    }
    test_bitexact("run62_boundary_4ch", img, 65, 1, 4);
  }
  {
    auto img = make_gradient(1920, 1080, 4);
    test_bitexact("gradient_1920x1080_4ch", img, 1920, 1080, 4);
  }

  printf("\nRoundtrip tests (turbo encode -> turbo decode):\n");
  {
    auto img = make_gradient(320, 240, 4);
    test_roundtrip("roundtrip_gradient_4ch", img, 320, 240, 4);
  }
  {
    auto img = make_gradient(320, 240, 3);
    test_roundtrip("roundtrip_gradient_3ch", img, 320, 240, 3);
  }
  {
    auto img = make_noise(320, 240, 4);
    test_roundtrip("roundtrip_noise_4ch", img, 320, 240, 4);
  }
  {
    auto img = make_solid(320, 240, 4);
    test_roundtrip("roundtrip_solid_4ch", img, 320, 240, 4);
  }

  printf("\nCross-roundtrip tests (turbo <-> reference):\n");
  {
    auto img = make_gradient(320, 240, 4);
    test_cross_roundtrip("cross_gradient_4ch", img, 320, 240, 4);
  }
  {
    auto img = make_noise(320, 240, 3);
    test_cross_roundtrip("cross_noise_3ch", img, 320, 240, 3);
  }
  {
    auto img = make_all_different(100, 100, 4);
    test_cross_roundtrip("cross_all_different_4ch", img, 100, 100, 4);
  }

  printf("\nRandom size bit-exact + roundtrip tests:\n");
  {
    uint32_t seed = 12345;
    for (int trial = 0; trial < 20; trial++) {
      seed = seed * 1103515245u + 12345u;
      int w = 1 + (seed >> 16) % 200;
      seed = seed * 1103515245u + 12345u;
      int h = 1 + (seed >> 16) % 200;
      int ch = (trial % 2 == 0) ? 4 : 3;

      auto img = make_noise(w, h, ch);
      char label[64];
      snprintf(label, sizeof(label), "fuzz_%dx%d_%dch", w, h, ch);
      test_bitexact(label, img, w, h, ch);
      test_roundtrip(label, img, w, h, ch);
    }
  }

  printf("\nMalformed stream tests:\n");
  {
    auto img = make_gradient(64, 64, 4);
    qoi_turbo::desc_t desc = {64, 64, 4, 0};
    auto encoded = qoi_turbo::encode(img.data(), desc);
    test_truncation("truncation_64x64_4ch", encoded);
  }

  printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}

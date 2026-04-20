#include <benchmark/benchmark.h>

#include <cstdint>
#include <cstring>
#include <vector>

#define QOI_IMPLEMENTATION
#include "qoi.h"
#include <qoi_turbo.hpp>

static std::vector<uint8_t> make_gradient(int w, int h) {
  std::vector<uint8_t> img(w * h * 4);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int i = (y * w + x) * 4;
      img[i + 0] = (x * 255) / (w - 1 > 0 ? w - 1 : 1);
      img[i + 1] = (y * 255) / (h - 1 > 0 ? h - 1 : 1);
      img[i + 2] = ((x + y) * 127) / (w + h - 2 > 0 ? w + h - 2 : 1);
      img[i + 3] = 255;
    }
  }
  return img;
}

static std::vector<uint8_t> make_noise(int w, int h) {
  std::vector<uint8_t> img(w * h * 4);
  uint32_t seed = 42;
  for (int i = 0; i < w * h * 4; i++) {
    seed = seed * 1103515245u + 12345u;
    img[i] = (seed >> 16) & 0xFF;
    if ((i & 3) == 3) {
      img[i] = 255;
    }
  }
  return img;
}

struct TestData {
  std::vector<uint8_t> pixels;
  std::vector<uint8_t> qoi_encoded;
  std::vector<uint8_t> turbo_encoded;
  int w, h;
  int64_t pixel_bytes;

  TestData(std::vector<uint8_t> px, int width, int height)
      : pixels(std::move(px)), w(width), h(height), pixel_bytes((int64_t)width * height * 4) {
    qoi_desc qdesc = {(unsigned)w, (unsigned)h, 4, 0};
    int len;
    void* enc = qoi_encode(pixels.data(), &qdesc, &len);
    qoi_encoded.assign((uint8_t*)enc, (uint8_t*)enc + len);
    QOI_FREE(enc);

    qoi_turbo::desc_t tdesc = {(unsigned)w, (unsigned)h, 4, 0};
    turbo_encoded.resize(qoi_turbo::encode_max_size(tdesc));
    int tlen = qoi_turbo::encode_to(pixels.data(), tdesc, turbo_encoded.data());
    turbo_encoded.resize(tlen);
  }
};

static TestData gradient_small(make_gradient(512, 512), 512, 512);
static TestData gradient_hd(make_gradient(1920, 1080), 1920, 1080);
static TestData noise_small(make_noise(512, 512), 512, 512);
static TestData noise_hd(make_noise(1920, 1080), 1920, 1080);

#define BENCH_ENCODE_REF(name, td)                                            \
  static void BM_Ref_Encode_##name(benchmark::State& state) {                 \
    qoi_desc desc = {(unsigned)td.w, (unsigned)td.h, 4, 0};                   \
    for (auto _ : state) {                                                    \
      int len;                                                                \
      void* enc = qoi_encode(td.pixels.data(), &desc, &len);                  \
      benchmark::DoNotOptimize(enc);                                          \
      QOI_FREE(enc);                                                          \
    }                                                                         \
    state.SetBytesProcessed(state.iterations() * td.pixel_bytes);             \
    state.counters["ratio"] = (double)td.qoi_encoded.size() / td.pixel_bytes; \
  }                                                                           \
  BENCHMARK(BM_Ref_Encode_##name)

#define BENCH_DECODE_REF(name, td)                                                    \
  static void BM_Ref_Decode_##name(benchmark::State& state) {                         \
    for (auto _ : state) {                                                            \
      qoi_desc desc;                                                                  \
      void* dec = qoi_decode(td.qoi_encoded.data(), td.qoi_encoded.size(), &desc, 4); \
      benchmark::DoNotOptimize(dec);                                                  \
      QOI_FREE(dec);                                                                  \
    }                                                                                 \
    state.SetBytesProcessed(state.iterations() * td.pixel_bytes);                     \
  }                                                                                   \
  BENCHMARK(BM_Ref_Decode_##name)

#define BENCH_ENCODE_TURBO(name, td)                                            \
  static void BM_Turbo_Encode_##name(benchmark::State& state) {                 \
    qoi_turbo::desc_t desc = {(unsigned)td.w, (unsigned)td.h, 4, 0};            \
    std::vector<uint8_t> buf(qoi_turbo::encode_max_size(desc));                 \
    for (auto _ : state) {                                                      \
      int len = qoi_turbo::encode_to(td.pixels.data(), desc, buf.data());       \
      benchmark::DoNotOptimize(buf.data());                                     \
      benchmark::DoNotOptimize(len);                                            \
    }                                                                           \
    state.SetBytesProcessed(state.iterations() * td.pixel_bytes);               \
    state.counters["ratio"] = (double)td.turbo_encoded.size() / td.pixel_bytes; \
  }                                                                             \
  BENCHMARK(BM_Turbo_Encode_##name)

#define BENCH_DECODE_TURBO(name, td)                                                        \
  static void BM_Turbo_Decode_##name(benchmark::State& state) {                             \
    qoi_turbo::desc_t desc = {(unsigned)td.w, (unsigned)td.h, 4, 0};                        \
    std::vector<uint8_t> buf(td.w* td.h * 4);                                               \
    for (auto _ : state) {                                                                  \
      qoi_turbo::decode_to(td.qoi_encoded.data(), td.qoi_encoded.size(), desc, buf.data()); \
      benchmark::DoNotOptimize(buf.data());                                                 \
    }                                                                                       \
    state.SetBytesProcessed(state.iterations() * td.pixel_bytes);                           \
  }                                                                                         \
  BENCHMARK(BM_Turbo_Decode_##name)

#define REGISTER_PAIR(name, td)   \
  BENCH_ENCODE_REF(name, td);     \
  BENCH_ENCODE_TURBO(name, td);   \
  BENCH_DECODE_REF(name, td);     \
  BENCH_DECODE_TURBO(name, td)

REGISTER_PAIR(GradientSmall, gradient_small);
REGISTER_PAIR(GradientHD, gradient_hd);
REGISTER_PAIR(NoiseSmall, noise_small);
REGISTER_PAIR(NoiseHD, noise_hd);

BENCHMARK_MAIN();

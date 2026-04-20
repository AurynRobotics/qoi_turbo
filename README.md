# qoi_turbo

**Bit-exact drop-in replacement for the reference [QOI](https://qoiformat.org/)
encoder/decoder, optimized for speed.**

Produces byte-identical output to the reference codec at roughly **1.5×
encode** and **1.2× decode** throughput on typical 8-bit RGB/RGBA images.
Header-only, C++17, no dependencies outside the standard library.

## Motivation

[QOI](https://qoiformat.org/) is an attractive format for applications that
need a simple, royalty-free, lossless image codec with a tiny implementation:
the whole format fits in a page and the reference encoder/decoder are each a
few hundred lines of C.

The reference implementation prioritises clarity over speed. For high-rate
workloads — realtime frame capture, asset pipelines, robotics logging — the
hot loop leaves a lot of ILP on the table. `qoi_turbo` is a rewrite of that
hot loop that keeps the bitstream exactly compatible with the reference
codec, so existing `.qoi` files and tools continue to work.

Encoded streams are bit-for-bit identical to the output of the reference
encoder; a bit-exact suite checks this against
[`phoboslab/qoi`](https://github.com/phoboslab/qoi) on every build.

## What changed vs. reference QOI

**Encoder:**
- Blend2D-style fast pixel hash — `(r·3 + g·5 + b·7 + a·11) mod 64` computed
  via a single 64-bit multiply + shift instead of 4 multiplies, an add, and
  a modulo.
- Fused DIFF / LUMA residual computation — both candidate residual forms are
  computed in parallel so the CPU picks the cheaper opcode with one
  branch chain instead of two.
- 2× manual unrolling of the per-pixel loop (~15% encoder speedup).
- Branchless DIFF range check: `(dr2 | dg2 | db2) < 4u` replaces six scalar
  comparisons.
- Auto-detection of constant alpha: on 4-channel input with all α = 255,
  the encoder switches to a template specialization that skips the alpha
  compare and never emits `OP_RGBA` (~12% on opaque RGBA).
- `__builtin_expect` hints on the hot opcode paths.

**Decoder:**
- Inline `OP_RUN` batch-fill with `memcpy` / 32-bit stores instead of a
  per-pixel run counter (~7% decoder speedup).
- Input prefetch (`__builtin_prefetch`) 64 bytes ahead in the hot loop.
- Opcode chain ordered by observed frequency (INDEX / DIFF / LUMA first).
- Separate 3-channel and 4-channel decode loops — the 4-channel loop stores
  pixels with `memcpy(..., 4)`, the 3-channel loop writes bytes directly.
- Truncation-safe: returns 0 on short/corrupted input.

## Usage

Header-only, C++17. The umbrella header pulls in everything:

```cpp
#include <qoi_turbo.hpp>

// --- Encode ---
qoi_turbo::desc_t desc = { width, height, /*channels=*/4, /*colorspace=*/0 };
std::vector<uint8_t> encoded = qoi_turbo::encode(pixels, desc);

// Or, zero-allocation variant:
std::vector<uint8_t> buf(qoi_turbo::encode_max_size(desc));
int len = qoi_turbo::encode_to(pixels, desc, buf.data());
buf.resize(len);

// --- Decode ---
qoi_turbo::desc_t out_desc;
std::vector<uint8_t> pixels = qoi_turbo::decode(encoded.data(),
                                                (int)encoded.size(),
                                                out_desc);

// Or, decode into a caller-owned buffer after reading the header:
qoi_turbo::desc_t d;
int px_bytes = qoi_turbo::decode_header(encoded.data(),
                                        (int)encoded.size(), d);
std::vector<uint8_t> out(px_bytes);
qoi_turbo::decode_to(encoded.data(), (int)encoded.size(), d, out.data());
```

### CMake

As a git submodule:

```bash
git submodule add https://github.com/AurynRobotics/qoi_turbo.git third_party/qoi_turbo
```

```cmake
add_subdirectory(third_party/qoi_turbo)
target_link_libraries(your_target PRIVATE qoi_turbo::qoi_turbo)
```

Or without CMake, just add `include/` to your include path and
`#include <qoi_turbo.hpp>`.

## Build (standalone)

The repo is buildable on its own; the tests pull
[`phoboslab/qoi`](https://github.com/phoboslab/qoi) via `FetchContent` to
verify bit-exactness, and the benchmark pulls
[`google/benchmark`](https://github.com/google/benchmark) the same way.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/qoi_turbo_bench
```

Pass `-DQOI_TURBO_BUILD_TESTS=OFF` / `-DQOI_TURBO_BUILD_BENCH=OFF` to skip
the fetched dependencies when consuming the repo as a submodule.

## Layout

```
include/
  qoi_turbo.hpp            - umbrella header
  qoi_turbo_common.hpp     - shared types (desc_t, rgba_t, pixel_hash_fast)
  qoi_turbo_encode.hpp     - encoder (encode, encode_to, encode_max_size)
  qoi_turbo_decode.hpp     - decoder (decode, decode_to, decode_header)
test/test_qoi_turbo.cpp    - roundtrip, bit-exact, cross-roundtrip, truncation
bench/bench_qoi_turbo.cpp  - reference-vs-turbo encode/decode throughput
```

## License

MIT — see [`LICENSE`](LICENSE).

#pragma once

// QOI Turbo — Optimized QOI encoder/decoder (bit-exact compatible)
//
// Drop-in replacement for the reference QOI codec that produces identical
// output at ~1.5x encode and ~1.2x decode speed.
//
// === Algorithm ===
//
// QOI (Quite OK Image) format with 5 opcodes, unchanged from the spec:
//   - OP_INDEX (1B): pixel matches hash table entry at index (0-63)
//   - OP_DIFF  (1B): R/G/B each differ by -2..+1 from previous pixel
//   - OP_LUMA  (2B): green differs by -32..+31, R-G and B-G by -8..+7
//   - OP_RUN   (1B): repeat previous pixel 1-62 times
//   - OP_RGB   (4B): explicit R, G, B values (alpha unchanged)
//   - OP_RGBA  (5B): explicit R, G, B, A values
//
// The encoder tries opcodes in order: RUN → INDEX → DIFF → LUMA → RGB/RGBA.
// A 64-entry hash table maps pixel values to indices for OP_INDEX lookups.
//
// === Encoder optimizations ===
//
// - Blend2D-style fast hash: (r*3 + g*5 + b*7 + a*11) % 64 computed via a
//   single 64-bit multiply and shift, replacing 4 multiplies + add + modulo
// - Fused DIFF+LUMA: both residual forms computed simultaneously for ILP;
//   the CPU evaluates DIFF and LUMA in parallel, choosing the cheaper one
// - 2x loop unrolling: process two pixels per iteration, reducing loop overhead
//   and branch mispredictions (+15% encoder speed)
// - Branchless DIFF range check: (dr2|dg2|db2) < 4u replaces 6 separate
//   comparisons with a single OR + unsigned compare
// - Auto-detect constant alpha: when all alpha values are 255, the encoder
//   skips alpha comparison and OP_RGBA entirely (template specialization,
//   +12% for RGB images)
// - __builtin_expect branch hints on likely/unlikely opcode paths
//
// === Decoder optimizations ===
//
// - Inline RUN batch-fill: OP_RUN fills N pixels with memcpy/loop instead of
//   decrementing a run counter per pixel (+7% decoder speed)
// - Input prefetch: __builtin_prefetch(data + p + 64) in the hot loop
// - Opcode ordering: if-chain ordered by frequency (INDEX/DIFF/LUMA most common)
//   with __builtin_expect hints for branch prediction
// - Separate 3-channel and 4-channel decode loops: the 4-channel loop uses
//   memcpy(&px.v, 4) for pixel output; the 3-channel loop writes bytes directly
// - Truncation safety: returns 0 if the decode loop exits early due to
//   truncated input (px_pos != px_len check)
//
// === Files ===
//
// - qoi_turbo.hpp: this umbrella header
// - qoi_turbo_common.hpp: shared types (desc_t, rgba_t, pixel_hash_fast)
// - qoi_turbo_encode.hpp: encoder (encode_to, encode, encode_max_size)
// - qoi_turbo_decode.hpp: decoder (decode_to, decode, decode_header)

#include "qoi_turbo_common.hpp"
#include "qoi_turbo_decode.hpp"
#include "qoi_turbo_encode.hpp"

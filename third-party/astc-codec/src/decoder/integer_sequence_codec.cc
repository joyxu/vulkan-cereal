// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/decoder/integer_sequence_codec.h"
#include "src/base/math_utils.h"
#include "src/base/utils.h"

#include <algorithm>
#include <iostream>

namespace astc_codec {

namespace {

// Tables of trit and quint encodings generated by the implementation in
// http://cs/aosp-master/external/skia/src/utils/SkTextureCompressor_ASTC.cpp
//
// These tables are used to decode the blocks of values encoded using the ASTC
// integer sequence encoding. The theory is that five trits (values that can
// take any number in the range [0, 2]) can take on a total of 3^5 = 243 total
// values, which can be stored in eight bits. These eight bits are used to
// decode the five trits based on the ASTC specification in Section C.2.12.
// For simplicity, we have stored a look-up table here so that we don't need
// to implement the decoding logic. Similarly, seven bits are used to decode
// three quints (since 5^3 = 125 < 128).
static const std::array<int, 5> kTritEncodings[256] = {
  {{ 0, 0, 0, 0, 0 }}, {{ 1, 0, 0, 0, 0 }}, {{ 2, 0, 0, 0, 0 }},
  {{ 0, 0, 2, 0, 0 }}, {{ 0, 1, 0, 0, 0 }}, {{ 1, 1, 0, 0, 0 }},
  {{ 2, 1, 0, 0, 0 }}, {{ 1, 0, 2, 0, 0 }}, {{ 0, 2, 0, 0, 0 }},
  {{ 1, 2, 0, 0, 0 }}, {{ 2, 2, 0, 0, 0 }}, {{ 2, 0, 2, 0, 0 }},
  {{ 0, 2, 2, 0, 0 }}, {{ 1, 2, 2, 0, 0 }}, {{ 2, 2, 2, 0, 0 }},
  {{ 2, 0, 2, 0, 0 }}, {{ 0, 0, 1, 0, 0 }}, {{ 1, 0, 1, 0, 0 }},
  {{ 2, 0, 1, 0, 0 }}, {{ 0, 1, 2, 0, 0 }}, {{ 0, 1, 1, 0, 0 }},
  {{ 1, 1, 1, 0, 0 }}, {{ 2, 1, 1, 0, 0 }}, {{ 1, 1, 2, 0, 0 }},
  {{ 0, 2, 1, 0, 0 }}, {{ 1, 2, 1, 0, 0 }}, {{ 2, 2, 1, 0, 0 }},
  {{ 2, 1, 2, 0, 0 }}, {{ 0, 0, 0, 2, 2 }}, {{ 1, 0, 0, 2, 2 }},
  {{ 2, 0, 0, 2, 2 }}, {{ 0, 0, 2, 2, 2 }}, {{ 0, 0, 0, 1, 0 }},
  {{ 1, 0, 0, 1, 0 }}, {{ 2, 0, 0, 1, 0 }}, {{ 0, 0, 2, 1, 0 }},
  {{ 0, 1, 0, 1, 0 }}, {{ 1, 1, 0, 1, 0 }}, {{ 2, 1, 0, 1, 0 }},
  {{ 1, 0, 2, 1, 0 }}, {{ 0, 2, 0, 1, 0 }}, {{ 1, 2, 0, 1, 0 }},
  {{ 2, 2, 0, 1, 0 }}, {{ 2, 0, 2, 1, 0 }}, {{ 0, 2, 2, 1, 0 }},
  {{ 1, 2, 2, 1, 0 }}, {{ 2, 2, 2, 1, 0 }}, {{ 2, 0, 2, 1, 0 }},
  {{ 0, 0, 1, 1, 0 }}, {{ 1, 0, 1, 1, 0 }}, {{ 2, 0, 1, 1, 0 }},
  {{ 0, 1, 2, 1, 0 }}, {{ 0, 1, 1, 1, 0 }}, {{ 1, 1, 1, 1, 0 }},
  {{ 2, 1, 1, 1, 0 }}, {{ 1, 1, 2, 1, 0 }}, {{ 0, 2, 1, 1, 0 }},
  {{ 1, 2, 1, 1, 0 }}, {{ 2, 2, 1, 1, 0 }}, {{ 2, 1, 2, 1, 0 }},
  {{ 0, 1, 0, 2, 2 }}, {{ 1, 1, 0, 2, 2 }}, {{ 2, 1, 0, 2, 2 }},
  {{ 1, 0, 2, 2, 2 }}, {{ 0, 0, 0, 2, 0 }}, {{ 1, 0, 0, 2, 0 }},
  {{ 2, 0, 0, 2, 0 }}, {{ 0, 0, 2, 2, 0 }}, {{ 0, 1, 0, 2, 0 }},
  {{ 1, 1, 0, 2, 0 }}, {{ 2, 1, 0, 2, 0 }}, {{ 1, 0, 2, 2, 0 }},
  {{ 0, 2, 0, 2, 0 }}, {{ 1, 2, 0, 2, 0 }}, {{ 2, 2, 0, 2, 0 }},
  {{ 2, 0, 2, 2, 0 }}, {{ 0, 2, 2, 2, 0 }}, {{ 1, 2, 2, 2, 0 }},
  {{ 2, 2, 2, 2, 0 }}, {{ 2, 0, 2, 2, 0 }}, {{ 0, 0, 1, 2, 0 }},
  {{ 1, 0, 1, 2, 0 }}, {{ 2, 0, 1, 2, 0 }}, {{ 0, 1, 2, 2, 0 }},
  {{ 0, 1, 1, 2, 0 }}, {{ 1, 1, 1, 2, 0 }}, {{ 2, 1, 1, 2, 0 }},
  {{ 1, 1, 2, 2, 0 }}, {{ 0, 2, 1, 2, 0 }}, {{ 1, 2, 1, 2, 0 }},
  {{ 2, 2, 1, 2, 0 }}, {{ 2, 1, 2, 2, 0 }}, {{ 0, 2, 0, 2, 2 }},
  {{ 1, 2, 0, 2, 2 }}, {{ 2, 2, 0, 2, 2 }}, {{ 2, 0, 2, 2, 2 }},
  {{ 0, 0, 0, 0, 2 }}, {{ 1, 0, 0, 0, 2 }}, {{ 2, 0, 0, 0, 2 }},
  {{ 0, 0, 2, 0, 2 }}, {{ 0, 1, 0, 0, 2 }}, {{ 1, 1, 0, 0, 2 }},
  {{ 2, 1, 0, 0, 2 }}, {{ 1, 0, 2, 0, 2 }}, {{ 0, 2, 0, 0, 2 }},
  {{ 1, 2, 0, 0, 2 }}, {{ 2, 2, 0, 0, 2 }}, {{ 2, 0, 2, 0, 2 }},
  {{ 0, 2, 2, 0, 2 }}, {{ 1, 2, 2, 0, 2 }}, {{ 2, 2, 2, 0, 2 }},
  {{ 2, 0, 2, 0, 2 }}, {{ 0, 0, 1, 0, 2 }}, {{ 1, 0, 1, 0, 2 }},
  {{ 2, 0, 1, 0, 2 }}, {{ 0, 1, 2, 0, 2 }}, {{ 0, 1, 1, 0, 2 }},
  {{ 1, 1, 1, 0, 2 }}, {{ 2, 1, 1, 0, 2 }}, {{ 1, 1, 2, 0, 2 }},
  {{ 0, 2, 1, 0, 2 }}, {{ 1, 2, 1, 0, 2 }}, {{ 2, 2, 1, 0, 2 }},
  {{ 2, 1, 2, 0, 2 }}, {{ 0, 2, 2, 2, 2 }}, {{ 1, 2, 2, 2, 2 }},
  {{ 2, 2, 2, 2, 2 }}, {{ 2, 0, 2, 2, 2 }}, {{ 0, 0, 0, 0, 1 }},
  {{ 1, 0, 0, 0, 1 }}, {{ 2, 0, 0, 0, 1 }}, {{ 0, 0, 2, 0, 1 }},
  {{ 0, 1, 0, 0, 1 }}, {{ 1, 1, 0, 0, 1 }}, {{ 2, 1, 0, 0, 1 }},
  {{ 1, 0, 2, 0, 1 }}, {{ 0, 2, 0, 0, 1 }}, {{ 1, 2, 0, 0, 1 }},
  {{ 2, 2, 0, 0, 1 }}, {{ 2, 0, 2, 0, 1 }}, {{ 0, 2, 2, 0, 1 }},
  {{ 1, 2, 2, 0, 1 }}, {{ 2, 2, 2, 0, 1 }}, {{ 2, 0, 2, 0, 1 }},
  {{ 0, 0, 1, 0, 1 }}, {{ 1, 0, 1, 0, 1 }}, {{ 2, 0, 1, 0, 1 }},
  {{ 0, 1, 2, 0, 1 }}, {{ 0, 1, 1, 0, 1 }}, {{ 1, 1, 1, 0, 1 }},
  {{ 2, 1, 1, 0, 1 }}, {{ 1, 1, 2, 0, 1 }}, {{ 0, 2, 1, 0, 1 }},
  {{ 1, 2, 1, 0, 1 }}, {{ 2, 2, 1, 0, 1 }}, {{ 2, 1, 2, 0, 1 }},
  {{ 0, 0, 1, 2, 2 }}, {{ 1, 0, 1, 2, 2 }}, {{ 2, 0, 1, 2, 2 }},
  {{ 0, 1, 2, 2, 2 }}, {{ 0, 0, 0, 1, 1 }}, {{ 1, 0, 0, 1, 1 }},
  {{ 2, 0, 0, 1, 1 }}, {{ 0, 0, 2, 1, 1 }}, {{ 0, 1, 0, 1, 1 }},
  {{ 1, 1, 0, 1, 1 }}, {{ 2, 1, 0, 1, 1 }}, {{ 1, 0, 2, 1, 1 }},
  {{ 0, 2, 0, 1, 1 }}, {{ 1, 2, 0, 1, 1 }}, {{ 2, 2, 0, 1, 1 }},
  {{ 2, 0, 2, 1, 1 }}, {{ 0, 2, 2, 1, 1 }}, {{ 1, 2, 2, 1, 1 }},
  {{ 2, 2, 2, 1, 1 }}, {{ 2, 0, 2, 1, 1 }}, {{ 0, 0, 1, 1, 1 }},
  {{ 1, 0, 1, 1, 1 }}, {{ 2, 0, 1, 1, 1 }}, {{ 0, 1, 2, 1, 1 }},
  {{ 0, 1, 1, 1, 1 }}, {{ 1, 1, 1, 1, 1 }}, {{ 2, 1, 1, 1, 1 }},
  {{ 1, 1, 2, 1, 1 }}, {{ 0, 2, 1, 1, 1 }}, {{ 1, 2, 1, 1, 1 }},
  {{ 2, 2, 1, 1, 1 }}, {{ 2, 1, 2, 1, 1 }}, {{ 0, 1, 1, 2, 2 }},
  {{ 1, 1, 1, 2, 2 }}, {{ 2, 1, 1, 2, 2 }}, {{ 1, 1, 2, 2, 2 }},
  {{ 0, 0, 0, 2, 1 }}, {{ 1, 0, 0, 2, 1 }}, {{ 2, 0, 0, 2, 1 }},
  {{ 0, 0, 2, 2, 1 }}, {{ 0, 1, 0, 2, 1 }}, {{ 1, 1, 0, 2, 1 }},
  {{ 2, 1, 0, 2, 1 }}, {{ 1, 0, 2, 2, 1 }}, {{ 0, 2, 0, 2, 1 }},
  {{ 1, 2, 0, 2, 1 }}, {{ 2, 2, 0, 2, 1 }}, {{ 2, 0, 2, 2, 1 }},
  {{ 0, 2, 2, 2, 1 }}, {{ 1, 2, 2, 2, 1 }}, {{ 2, 2, 2, 2, 1 }},
  {{ 2, 0, 2, 2, 1 }}, {{ 0, 0, 1, 2, 1 }}, {{ 1, 0, 1, 2, 1 }},
  {{ 2, 0, 1, 2, 1 }}, {{ 0, 1, 2, 2, 1 }}, {{ 0, 1, 1, 2, 1 }},
  {{ 1, 1, 1, 2, 1 }}, {{ 2, 1, 1, 2, 1 }}, {{ 1, 1, 2, 2, 1 }},
  {{ 0, 2, 1, 2, 1 }}, {{ 1, 2, 1, 2, 1 }}, {{ 2, 2, 1, 2, 1 }},
  {{ 2, 1, 2, 2, 1 }}, {{ 0, 2, 1, 2, 2 }}, {{ 1, 2, 1, 2, 2 }},
  {{ 2, 2, 1, 2, 2 }}, {{ 2, 1, 2, 2, 2 }}, {{ 0, 0, 0, 1, 2 }},
  {{ 1, 0, 0, 1, 2 }}, {{ 2, 0, 0, 1, 2 }}, {{ 0, 0, 2, 1, 2 }},
  {{ 0, 1, 0, 1, 2 }}, {{ 1, 1, 0, 1, 2 }}, {{ 2, 1, 0, 1, 2 }},
  {{ 1, 0, 2, 1, 2 }}, {{ 0, 2, 0, 1, 2 }}, {{ 1, 2, 0, 1, 2 }},
  {{ 2, 2, 0, 1, 2 }}, {{ 2, 0, 2, 1, 2 }}, {{ 0, 2, 2, 1, 2 }},
  {{ 1, 2, 2, 1, 2 }}, {{ 2, 2, 2, 1, 2 }}, {{ 2, 0, 2, 1, 2 }},
  {{ 0, 0, 1, 1, 2 }}, {{ 1, 0, 1, 1, 2 }}, {{ 2, 0, 1, 1, 2 }},
  {{ 0, 1, 2, 1, 2 }}, {{ 0, 1, 1, 1, 2 }}, {{ 1, 1, 1, 1, 2 }},
  {{ 2, 1, 1, 1, 2 }}, {{ 1, 1, 2, 1, 2 }}, {{ 0, 2, 1, 1, 2 }},
  {{ 1, 2, 1, 1, 2 }}, {{ 2, 2, 1, 1, 2 }}, {{ 2, 1, 2, 1, 2 }},
  {{ 0, 2, 2, 2, 2 }}, {{ 1, 2, 2, 2, 2 }}, {{ 2, 2, 2, 2, 2 }},
  {{ 2, 1, 2, 2, 2 }}
};

static const std::array<int, 3> kQuintEncodings[128] = {
  {{ 0, 0, 0 }}, {{ 1, 0, 0 }}, {{ 2, 0, 0 }}, {{ 3, 0, 0 }}, {{ 4, 0, 0 }},
  {{ 0, 4, 0 }}, {{ 4, 4, 0 }}, {{ 4, 4, 4 }}, {{ 0, 1, 0 }}, {{ 1, 1, 0 }},
  {{ 2, 1, 0 }}, {{ 3, 1, 0 }}, {{ 4, 1, 0 }}, {{ 1, 4, 0 }}, {{ 4, 4, 1 }},
  {{ 4, 4, 4 }}, {{ 0, 2, 0 }}, {{ 1, 2, 0 }}, {{ 2, 2, 0 }}, {{ 3, 2, 0 }},
  {{ 4, 2, 0 }}, {{ 2, 4, 0 }}, {{ 4, 4, 2 }}, {{ 4, 4, 4 }}, {{ 0, 3, 0 }},
  {{ 1, 3, 0 }}, {{ 2, 3, 0 }}, {{ 3, 3, 0 }}, {{ 4, 3, 0 }}, {{ 3, 4, 0 }},
  {{ 4, 4, 3 }}, {{ 4, 4, 4 }}, {{ 0, 0, 1 }}, {{ 1, 0, 1 }}, {{ 2, 0, 1 }},
  {{ 3, 0, 1 }}, {{ 4, 0, 1 }}, {{ 0, 4, 1 }}, {{ 4, 0, 4 }}, {{ 0, 4, 4 }},
  {{ 0, 1, 1 }}, {{ 1, 1, 1 }}, {{ 2, 1, 1 }}, {{ 3, 1, 1 }}, {{ 4, 1, 1 }},
  {{ 1, 4, 1 }}, {{ 4, 1, 4 }}, {{ 1, 4, 4 }}, {{ 0, 2, 1 }}, {{ 1, 2, 1 }},
  {{ 2, 2, 1 }}, {{ 3, 2, 1 }}, {{ 4, 2, 1 }}, {{ 2, 4, 1 }}, {{ 4, 2, 4 }},
  {{ 2, 4, 4 }}, {{ 0, 3, 1 }}, {{ 1, 3, 1 }}, {{ 2, 3, 1 }}, {{ 3, 3, 1 }},
  {{ 4, 3, 1 }}, {{ 3, 4, 1 }}, {{ 4, 3, 4 }}, {{ 3, 4, 4 }}, {{ 0, 0, 2 }},
  {{ 1, 0, 2 }}, {{ 2, 0, 2 }}, {{ 3, 0, 2 }}, {{ 4, 0, 2 }}, {{ 0, 4, 2 }},
  {{ 2, 0, 4 }}, {{ 3, 0, 4 }}, {{ 0, 1, 2 }}, {{ 1, 1, 2 }}, {{ 2, 1, 2 }},
  {{ 3, 1, 2 }}, {{ 4, 1, 2 }}, {{ 1, 4, 2 }}, {{ 2, 1, 4 }}, {{ 3, 1, 4 }},
  {{ 0, 2, 2 }}, {{ 1, 2, 2 }}, {{ 2, 2, 2 }}, {{ 3, 2, 2 }}, {{ 4, 2, 2 }},
  {{ 2, 4, 2 }}, {{ 2, 2, 4 }}, {{ 3, 2, 4 }}, {{ 0, 3, 2 }}, {{ 1, 3, 2 }},
  {{ 2, 3, 2 }}, {{ 3, 3, 2 }}, {{ 4, 3, 2 }}, {{ 3, 4, 2 }}, {{ 2, 3, 4 }},
  {{ 3, 3, 4 }}, {{ 0, 0, 3 }}, {{ 1, 0, 3 }}, {{ 2, 0, 3 }}, {{ 3, 0, 3 }},
  {{ 4, 0, 3 }}, {{ 0, 4, 3 }}, {{ 0, 0, 4 }}, {{ 1, 0, 4 }}, {{ 0, 1, 3 }},
  {{ 1, 1, 3 }}, {{ 2, 1, 3 }}, {{ 3, 1, 3 }}, {{ 4, 1, 3 }}, {{ 1, 4, 3 }},
  {{ 0, 1, 4 }}, {{ 1, 1, 4 }}, {{ 0, 2, 3 }}, {{ 1, 2, 3 }}, {{ 2, 2, 3 }},
  {{ 3, 2, 3 }}, {{ 4, 2, 3 }}, {{ 2, 4, 3 }}, {{ 0, 2, 4 }}, {{ 1, 2, 4 }},
  {{ 0, 3, 3 }}, {{ 1, 3, 3 }}, {{ 2, 3, 3 }}, {{ 3, 3, 3 }}, {{ 4, 3, 3 }},
  {{ 3, 4, 3 }}, {{ 0, 3, 4 }}, {{ 1, 3, 4 }}
};

// A cached table containing the max ranges for values encoded using ASTC's
// Bounded Integer Sequence Encoding. These are the numbers between 1 and 255
// that can be represented exactly as a number in the ranges
// [0, 2^k), [0, 3 * 2^k), and [0, 5 * 2^k).
static const std::array<int, kNumPossibleRanges> kMaxRanges = []() {
  std::array<int, kNumPossibleRanges> ranges;

  // Initialize the table that we need for determining value encodings.
  auto next_max_range = ranges.begin();
  auto add_val = [&next_max_range](int val) {
    if (val <= 0 || (1 << kLog2MaxRangeForBits) <= val) {
      return;
    }

    *(next_max_range++) = val;
  };

  for (int i = 0; i <= kLog2MaxRangeForBits; ++i) {
    add_val(3 * (1 << i) - 1);
    add_val(5 * (1 << i) - 1);
    add_val((1 << i) - 1);
  }

  assert(std::distance(next_max_range, ranges.end()) == 0);
  std::sort(ranges.begin(), ranges.end());
  return ranges;
}();

// Returns true if x == 0 or if x is a power of two. This function is only used
// in the GetCountsForRange function, where we need to have it return true
// on zero since we can have single trit/quint ISE encodings according to
// Table C.2.7.
template<typename T,
         typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
inline constexpr bool IsPow2(T x) { return (x & (x - 1)) == 0; }

// For the ISE block encoding, these arrays determine how many bits are
// used after each value to store the interleaved quint/trit block.
const int kInterleavedQuintBits[3] = { 3, 2, 2 };
const int kInterleavedTritBits[5] = { 2, 2, 1, 2, 1 };

// Some template meta programming to get around the fact that MSVC
// will not allow  (ValRange == 5) ? 3 : 5 as a template parameter
template<int ValRange>
struct DecodeBlockSize {
  enum { value =  (ValRange == 5 ? 3 : 5) };
};

// Decodes either a trit or quint block using the BISE (Bounded Integer Sequence
// Encoding) defined in Section C.2.12 of the ASTC specification. ValRange is
// expected to be either 3 or 5 depending on whether or not we're encoding trits
// or quints respectively. In other words, it is the remaining factor in whether
// the passed blocks contain encoded values of the form 3*2^k or 5*2^k.
template<int ValRange>
std::array<int, /* kNumVals = */ DecodeBlockSize<ValRange>::value> DecodeISEBlock(
    uint64_t block_bits, int num_bits) {
  static_assert(ValRange == 3 || ValRange == 5,
                "We only know about trits and quints");

  // We either have three quints or five trits
  constexpr const int kNumVals = (ValRange == 5) ? 3 : 5;

  // Depending on whether or not we're using quints or trits will determine
  // the positions of the interleaved bits in the encoded block.
  constexpr const int* const kInterleavedBits =
      (ValRange == 5) ? kInterleavedQuintBits : kInterleavedTritBits;

  // Set up the bits for reading
  base::BitStream<base::UInt128> block_bit_src(block_bits, sizeof(block_bits) * 8);

  // Decode the block
  std::array<int, kNumVals> m;
  uint64_t encoded = 0;
  uint32_t encoded_bits_read = 0;
  for (int i = 0; i < kNumVals; ++i) {
    {
      uint64_t bits = 0;
      const bool result = block_bit_src.GetBits(num_bits, &bits);
      assert(result);

      m[i] = static_cast<int>(bits);
    }

    uint64_t encoded_bits;
    {
      const bool result = block_bit_src.GetBits(kInterleavedBits[i], &encoded_bits);
      assert(result);
    }
    encoded |= encoded_bits << encoded_bits_read;
    encoded_bits_read += kInterleavedBits[i];
  }

  // Make sure that our encoded trit/quint doesn't exceed its bounds
  assert(ValRange != 3 || encoded < 256);
  assert(ValRange != 5 || encoded < 128);

  const int* const kEncodings = (ValRange == 5) ?
      kQuintEncodings[encoded].data() : kTritEncodings[encoded].data();

  std::array<int, kNumVals> result;
  for (int i = 0; i < kNumVals; ++i) {
    assert(m[i] < 1 << num_bits);
    result[i] = kEncodings[i] << num_bits | m[i];
  }
  return result;
}

// Encode a single trit or quint block using the BISE (Bounded Integer Sequence
// Encoding) defined in Section C.2.12 of the ASTC specification. ValRange is
// expected to be either 3 or 5 depending on whether or not we're encoding trits
// or quints respectively. In other words, it is the remaining factor in whether
// the passed blocks contain encoded values of the form 3*2^k or 5*2^k.
template <int ValRange>
void EncodeISEBlock(const std::vector<int>& vals, int bits_per_val,
                    base::BitStream<base::UInt128>* bit_sink) {
  static_assert(ValRange == 3 || ValRange == 5,
                "We only know about trits and quints");

  // We either have three quints or five trits
  constexpr const int kNumVals = (ValRange == 5) ? 3 : 5;

  // Three quints in seven bits or five trits in eight bits
  constexpr const int kNumEncodedBitsPerBlock = (ValRange == 5) ? 7 : 8;

  // Depending on whether or not we're using quints or trits will determine
  // the positions of the interleaved bits in the encoding
  constexpr const int* const kInterleavedBits =
      (ValRange == 5) ? kInterleavedQuintBits : kInterleavedTritBits;

  // ISE blocks can only have up to a specific number of values...
  assert(vals.size() <= kNumVals);

  // Split up into bits and non bits. Non bits are used to find the quint/trit
  // encoding that we need.
  std::array<int, kNumVals> non_bits = {{ 0 }};
  std::array<int, kNumVals> bits = {{ 0 }};
  for (size_t i = 0; i < vals.size(); ++i) {
    bits[i] = vals[i] & ((1 << bits_per_val) - 1);
    non_bits[i] = vals[i] >> bits_per_val;
    assert(non_bits[i] < ValRange);
  }

  // We only need to add as many bits as necessary, so let's limit it based
  // on the computation described in Section C.2.22 of the ASTC specification
  const int total_num_bits =
      ((vals.size() * kNumEncodedBitsPerBlock + kNumVals - 1) / kNumVals)
      + vals.size() * bits_per_val;
  int bits_added = 0;

  // The number of bits used for the quint/trit encoding is necessary to know
  // in order to properly select the encoding we need to represent.
  int num_encoded_bits = 0;
  for (int i = 0; i < kNumVals; ++i) {
    bits_added += bits_per_val;
    if (bits_added >= total_num_bits) {
      break;
    }

    num_encoded_bits += kInterleavedBits[i];
    bits_added += kInterleavedBits[i];
    if (bits_added >= total_num_bits) {
      break;
    }
  }
  bits_added = 0;
  assert(num_encoded_bits <= kNumEncodedBitsPerBlock);

  // TODO(google): The faster way to do this would be to construct trees out
  // of the quint/trit encoding patterns, or just invert the decoding logic.
  // Here we go from the end backwards because it makes our tests are more
  // deterministic.
  int non_bit_encoding = -1;
  for (int j = (1 << num_encoded_bits) - 1; j >= 0; --j) {
    bool matches = true;

    // We don't need to match all trits here, just the ones that correspond
    // to the values that we passed in
    for (size_t i = 0; i < kNumVals; ++i) {
      if ((ValRange == 5 && kQuintEncodings[j][i] != non_bits[i]) ||
          (ValRange == 3 && kTritEncodings[j][i] != non_bits[i])) {
        matches = false;
        break;
      }
    }

    if (matches) {
      non_bit_encoding = j;
      break;
    }
  }

  assert(non_bit_encoding >= 0);

  // Now pack the bits into the block
  for (int i = 0; i < vals.size(); ++i) {
    // First add the base bits for this value
    if (bits_added + bits_per_val <= total_num_bits) {
      bit_sink->PutBits(bits[i], bits_per_val);
      bits_added += bits_per_val;
    }

    // Now add the interleaved bits from the quint/trit
    int num_int_bits = kInterleavedBits[i];
    int int_bits = non_bit_encoding & ((1 << num_int_bits) - 1);
    if (bits_added + num_int_bits <= total_num_bits) {
      bit_sink->PutBits(int_bits, num_int_bits);
      bits_added += num_int_bits;
      non_bit_encoding >>= num_int_bits;
    }
  }
}

inline void CHECK_COUNTS(int trits, int quints) {
  assert(trits == 0 || quints == 0);   // Either trits or quints
  assert(trits == 0 || trits == 1);    // At most one trit
  assert(quints == 0 || quints == 1);  // At most one quint
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

std::array<int, kNumPossibleRanges>::const_iterator ISERangeBegin() {
  return kMaxRanges.cbegin();
}

std::array<int, kNumPossibleRanges>::const_iterator ISERangeEnd() {
  return kMaxRanges.cend();
}

void IntegerSequenceCodec::GetCountsForRange(
    int range, int* const trits, int* const quints, int* const bits) {
  // Make sure the passed pointers are valid
  assert(trits != nullptr);
  assert(quints != nullptr);
  assert(bits != nullptr);

  // These are generally errors -- there should never be any ASTC values
  // outside of this range
  UTILS_RELEASE_ASSERT(range > 0);
  UTILS_RELEASE_ASSERT(range < 1 << kLog2MaxRangeForBits);

  *bits = 0;
  *trits = 0;
  *quints = 0;

  // Search through the numbers of the form 2^n, 3 * 2^n and 5 * 2^n
  const int max_vals_for_range =
      *std::lower_bound(kMaxRanges.begin(), kMaxRanges.end(), range) + 1;

  // Make sure we found something
  assert(max_vals_for_range > 1);

  // Find out what kind of range it is
  if ((max_vals_for_range % 3 == 0) && IsPow2(max_vals_for_range / 3)) {
    *bits = base::Log2Floor(max_vals_for_range / 3);
    *trits = 1;
    *quints = 0;
  } else if ((max_vals_for_range % 5 == 0) && IsPow2(max_vals_for_range / 5)) {
    *bits = base::Log2Floor(max_vals_for_range / 5);
    *trits = 0;
    *quints = 1;
  } else if (IsPow2(max_vals_for_range)) {
    *bits = base::Log2Floor(max_vals_for_range);
    *trits = 0;
    *quints = 0;
  }

  // If we set any of these values then we're done.
  if ((*bits | *trits | *quints) != 0) {
    CHECK_COUNTS(*trits, *quints);
  }
}

// Returns the overall bit count for a range of val_count values encoded
// using the specified number of trits, quints and straight bits (respectively)
int IntegerSequenceCodec::GetBitCount(int num_vals,
                                      int trits, int quints, int bits) {
  CHECK_COUNTS(trits, quints);

  // See section C.2.22 for the formula used here.
  const int trit_bit_count = ((num_vals * 8 * trits) + 4) / 5;
  const int quint_bit_count = ((num_vals * 7 * quints) + 2) / 3;
  const int base_bit_count = num_vals * bits;
  return trit_bit_count + quint_bit_count + base_bit_count;
}

IntegerSequenceCodec::IntegerSequenceCodec(int range) {
  int trits, quints, bits;
  GetCountsForRange(range, &trits, &quints, &bits);
  InitializeWithCounts(trits, quints, bits);
}

IntegerSequenceCodec::IntegerSequenceCodec(
    int trits, int quints, int bits) {
  InitializeWithCounts(trits, quints, bits);
}

void IntegerSequenceCodec::InitializeWithCounts(
    int trits, int quints, int bits) {
  CHECK_COUNTS(trits, quints);

  if (trits > 0) {
    encoding_ = EncodingMode::kTritEncoding;
  } else if (quints > 0) {
    encoding_ = EncodingMode::kQuintEncoding;
  } else {
    encoding_ = EncodingMode::kBitEncoding;
  }

  bits_ = bits;
}

int IntegerSequenceCodec::NumValsPerBlock() const {
  const std::array<int, 3> kNumValsByEncoding = {{ 5, 3, 1 }};
  return kNumValsByEncoding[static_cast<int>(encoding_)];
}

int IntegerSequenceCodec::EncodedBlockSize() const {
  const std::array<int, 3> kExtraBlockSizeByEncoding = {{ 8, 7, 0 }};
  const int num_vals = NumValsPerBlock();
  return kExtraBlockSizeByEncoding[static_cast<int>(encoding_)]
      + num_vals * bits_;
}

std::vector<int> IntegerSequenceDecoder::Decode(
    int num_vals, base::BitStream<base::UInt128> *bit_src) const {
  int trits = (encoding_ == kTritEncoding)? 1 : 0;
  int quints = (encoding_ == kQuintEncoding)? 1 : 0;
  const int total_num_bits = GetBitCount(num_vals, trits, quints, bits_);
  const int bits_per_block = EncodedBlockSize();
  assert(bits_per_block < 64);

  int bits_left = total_num_bits;
  std::vector<int> result;
  while (bits_left > 0) {
    uint64_t block_bits;
    {
      const bool result = bit_src->GetBits(std::min(bits_left, bits_per_block), &block_bits);
      assert(result);
    }

    switch (encoding_) {
      case kTritEncoding: {
        auto trit_vals = DecodeISEBlock<3>(block_bits, bits_);
        result.insert(result.end(), trit_vals.begin(), trit_vals.end());
      }
      break;

      case kQuintEncoding: {
        auto quint_vals = DecodeISEBlock<5>(block_bits, bits_);
        result.insert(result.end(), quint_vals.begin(), quint_vals.end());
      }
      break;

      case kBitEncoding:
        result.push_back(static_cast<int>(block_bits));
        break;
    }

    bits_left -= bits_per_block;
  }

  // Resize result to only contain as many values as requested
  assert(result.size() >= static_cast<size_t>(num_vals));
  result.resize(num_vals);

  // Encoded all the values
  return result;
}

void IntegerSequenceEncoder::Encode(base::BitStream<base::UInt128>* bit_sink) const {
  // Go through all of the values and chop them up into blocks. The properties
  // of the trit and quint encodings mean that if we need to encode fewer values
  // in a block than the number of values encoded in the block then we need to
  // consider the last few values to be zero.

  auto next_val = vals_.begin();
  while (next_val != vals_.end()) {
    switch (encoding_) {
      case kTritEncoding: {
        std::vector<int> trit_vals;
        for (int i = 0; i < 5; ++i) {
          if (next_val != vals_.end()) {
            trit_vals.push_back(*next_val);
            ++next_val;
          }
        }

        EncodeISEBlock<3>(trit_vals, bits_, bit_sink);
      }
      break;

      case kQuintEncoding: {
        std::vector<int> quint_vals;
        for (int i = 0; i < 3; ++i) {
          if (next_val != vals_.end()) {
            quint_vals.push_back(*next_val);
            ++next_val;
          }
        }

        EncodeISEBlock<5>(quint_vals, bits_, bit_sink);
      }
      break;

      case kBitEncoding: {
        bit_sink->PutBits(*next_val, EncodedBlockSize());
        ++next_val;
      }
      break;
    }
  }
}

}  // namespace astc_codec

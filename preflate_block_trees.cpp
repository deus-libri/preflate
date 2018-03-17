/* Copyright 2018 Dirk Steinke

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#include <algorithm>
#include "preflate_block_trees.h"
#include "support/bit_helper.h"

static HuffmanDecoder* staticLitLenDecoder;
static HuffmanDecoder* staticDistDecoder;
static HuffmanEncoder* staticLitLenEncoder;
static HuffmanEncoder* staticDistEncoder;

static void setLitLenBitLengths(unsigned char(&a)[288]) {
  std::fill(a +   0, a + 144, 8);
  std::fill(a + 144, a + 256, 9);
  std::fill(a + 256, a + 280, 7);
  std::fill(a + 280, a + 288, 8);
}
static void setDistBitLengths(unsigned char(&a)[32]) {
  std::fill(a, a + 32, 5);
}

const HuffmanDecoder* PreflateBlockTrees::staticLitLenTreeDecoder() {
  if (!staticLitLenDecoder) {
    unsigned char l_lengths[288];
    setLitLenBitLengths(l_lengths);
    staticLitLenDecoder = new HuffmanDecoder(l_lengths, 288, true, 15);
  }
  return staticLitLenDecoder;
}
const HuffmanDecoder* PreflateBlockTrees::staticDistTreeDecoder() {
  if (!staticDistDecoder) {
    unsigned char d_lengths[32];
    setDistBitLengths(d_lengths);
    staticDistDecoder = new HuffmanDecoder(d_lengths, 32, true, 15);
  }
  return staticDistDecoder;
}
const HuffmanEncoder* PreflateBlockTrees::staticLitLenTreeEncoder() {
  if (!staticLitLenEncoder) {
    unsigned char l_lengths[288];
    setLitLenBitLengths(l_lengths);
    staticLitLenEncoder = new HuffmanEncoder(l_lengths, 288, true);
  }
  return staticLitLenEncoder;
}
const HuffmanEncoder* PreflateBlockTrees::staticDistTreeEncoder() {
  if (!staticDistEncoder) {
    unsigned char d_lengths[32];
    setDistBitLengths(d_lengths);
    staticDistEncoder = new HuffmanEncoder(d_lengths, 32, true);
  }
  return staticDistEncoder;
}

bool PreflateBlockTrees::unpackTreeCodes(
    const std::vector<unsigned char>& treecodes,
    const unsigned ncode,
    const unsigned nlen,
    const unsigned ndist) {
  if (ncode < 4 || ncode > PreflateConstants::BL_CODES
      || nlen < PreflateConstants::LITERALS + 1
      || nlen > PreflateConstants::L_CODES
      || ndist > PreflateConstants::D_CODES) {
    return false;
  }

  memset(litLenDistBitStorage, 0, sizeof(litLenDistBitStorage));
  memset(treeBitStorage, 0, sizeof(treeBitStorage));
  for (unsigned i = 0; i < ncode; ++i) {
    unsigned char tc = treecodes[i];
    if (tc > 7) {
      return false;
    }
    treeBitStorage[PreflateConstants::treeCodeOrderTable[i]] = tc;
  }
  unsigned o = 0;
  for (unsigned i = ncode, n = treecodes.size(); i < n; ++i) {
    unsigned char code = treecodes[i];
    if (code > 18) {
      return false;
    }
    if (code < 16) {
      if (o >= PreflateConstants::LD_CODES) {
        return false;
      }
      litLenDistBitStorage[o++] = code;
    } else {
      if (i + 1 >= n) {
        return false;
      }
      unsigned char len = treecodes[++i];
      if (o + len > PreflateConstants::LD_CODES) {
        return false;
      }
      unsigned char tocopy = code == 16 ? litLenDistBitStorage[o - 1] : 0;
      while (len-- > 0) {
        litLenDistBitStorage[o++] = tocopy;
      }
    }
  }
  litLenBits = litLenDistBitStorage;
  distBits = litLenDistBitStorage + nlen;
  treeBits = treeBitStorage;
  return o == nlen + ndist;
}
bool PreflateBlockTrees::generateTreeBitCodes(
    unsigned short* codes,
    const unsigned char* lengths,
    const unsigned size) {
  unsigned short bl_count[PreflateConstants::MAX_BITS + 1];
  unsigned short next_code[PreflateConstants::MAX_BITS + 1];

  memset(bl_count, 0, sizeof(bl_count));
  for (unsigned i = 0; i < size; ++i) {
    bl_count[lengths[i]]++;
  }
  bl_count[0] = 0;
  unsigned code = 0;
  for (unsigned i = 1; i <= PreflateConstants::MAX_BITS; ++i) {
    code = (code + bl_count[i - 1]) << 1;
    next_code[i] = (unsigned short)code;
  }
  if (next_code[PreflateConstants::MAX_BITS] + bl_count[PreflateConstants::MAX_BITS] != 32768) {
    return false;
  }
  for (unsigned i = 0; i < size; ++i) {
    unsigned char len = lengths[i];
    if (!len) {
      codes[i] = 0;
      continue;
    }
    codes[i] = bitReverse(next_code[len]++, len);
  }
  return true;
}
bool PreflateBlockTrees::generateAllTreeBitCodes(const unsigned nlen, const unsigned ndist) {
  if (!generateTreeBitCodes(treeCodeStorage, treeBitStorage, PreflateConstants::BL_CODES)) {
    return false;
  }
  treeCode = treeCodeStorage;
  if (!generateTreeBitCodes(litLenDistCodeStorage, litLenDistBitStorage, nlen)) {
    return false;
  }
  litLenCode = litLenDistCodeStorage;
  if (!generateTreeBitCodes(litLenDistCodeStorage + nlen, litLenDistBitStorage + nlen, ndist)) {
    return false;
  }
  distCode = litLenDistCodeStorage + nlen;
  return true;
}

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
#include "preflate_block_reencoder.h"
#include "support/bit_helper.h"

PreflateBlockReencoder::PreflateBlockReencoder(const std::vector<unsigned char>& dump)
  : bufferpos(0)
  , bitbuffer(0)
  , bitbuffersize(0)
  , input(dump) {
}

void PreflateBlockReencoder::unpackTreeCodes(
    const std::vector<unsigned char>& treecodes, 
    const unsigned ncode,
    const unsigned nlen) {
  memset(litLenDistBitStorage, 0, sizeof(litLenDistBitStorage));
  memset(treeBitStorage, 0, sizeof(treeBitStorage));
  for (unsigned i = 0; i < ncode; ++i) {
    treeBitStorage[PreflateConstants::treeCodeOrderTable[i]] = treecodes[i];
  }
  for (unsigned i = ncode, o = 0; i < treecodes.size(); ++i) {
    unsigned char code = treecodes[i];
    if (code < 16) {
      litLenDistBitStorage[o++] = code;
    } else {
      unsigned char len = treecodes[++i];
      unsigned char tocopy = code == 16 ? litLenDistBitStorage[o - 1] : 0;
      while (len-- > 0) {
        litLenDistBitStorage[o++] = tocopy;
      }
    }
  }
  litLenBits = litLenDistBitStorage;
  distBits = litLenDistBitStorage + nlen;
  treeBits = treeBitStorage;
}
void PreflateBlockReencoder::generateTreeBitCodes(
    unsigned short* codes,
    const unsigned char* lengths,
    const unsigned size) {
  unsigned short bl_count[16];
  unsigned short next_code[16];

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
  for (unsigned i = 0; i < size; ++i) {
    unsigned char len = lengths[i];
    if (!len) {
      codes[i] = 0;
      continue;
    }
    codes[i] = bitReverse(next_code[len]++, len);
  }
}
void PreflateBlockReencoder::generateAllTreeBitCodes(const unsigned nlen, const unsigned ndist) {
  generateTreeBitCodes(treeCodeStorage, treeBitStorage, PreflateConstants::BL_CODES);
  treeCode = treeCodeStorage;
  generateTreeBitCodes(litLenDistCodeStorage, litLenDistBitStorage, nlen);
  litLenCode = litLenDistCodeStorage;
  generateTreeBitCodes(litLenDistCodeStorage + nlen, litLenDistBitStorage + nlen, ndist);
  distCode = litLenDistCodeStorage + nlen;
}

void PreflateBlockReencoder::writeTrees(const std::vector<unsigned char>& treecodes, const unsigned tccount) {
  for (unsigned i = 0; i < tccount; ++i) {
    writeBits(treecodes[i], 3);
  }
  for (unsigned i = tccount, n = treecodes.size(); i < n; ++i) {
    unsigned char code = treecodes[i], len;
    writeBits(treeCode[code], treeBits[code]);
    if (code >= 16) {
      static unsigned char repExtraBits[3] = {2, 3, 7};
      static unsigned char repOffset[3] = {3, 3, 11};
      len = treecodes[++i];
      writeBits(len - repOffset[code - 16], repExtraBits[code - 16]);
    }
  }
}

void PreflateBlockReencoder::writeTokens(const std::vector<PreflateToken>& tokens) {
  for (unsigned i = 0; i < tokens.size(); ++i) {
    PreflateToken token = tokens[i];
    if (token.len == 1) {
      unsigned literal = input.curChar();
      writeBits(litLenCode[literal], litLenBits[literal]);
      input.advance(1);
    } else {
      unsigned lencode = PreflateConstants::LCode(token.len);
      writeBits(litLenCode[PreflateConstants::LITERALS + 1 + lencode], 
                litLenBits[PreflateConstants::LITERALS + 1 + lencode]);
      unsigned lenextra = PreflateConstants::lengthExtraTable[lencode];
      if (lenextra) {
        writeBits(token.len - PreflateConstants::MIN_MATCH - PreflateConstants::lengthBaseTable[lencode], lenextra);
      }
      unsigned distcode = PreflateConstants::DCode(token.dist);
      writeBits(distCode[distcode], distBits[distcode]);
      unsigned distextra = PreflateConstants::distExtraTable[distcode];
      if (distextra) {
        writeBits(token.dist - 1 - PreflateConstants::distBaseTable[distcode], distextra);
      }
      input.advance(token.len);
    }
  }
  writeBits(litLenCode[256], litLenBits[256]);
}

void PreflateBlockReencoder::writeBlock(const PreflateTokenBlock& block, bool last) {
  writeBits(last, 1); //
  switch (block.type) {
  case PreflateTokenBlock::DYNAMIC_HUFF:
    writeBits(2, 2); //
    writeBits(block.nlen - PreflateConstants::LITERALS - 1, 5);
    writeBits(block.ndist - 1, 5);
    writeBits(block.ncode - 4, 4);
    unpackTreeCodes(block.treecodes, block.ncode, block.nlen);
    generateAllTreeBitCodes(block.nlen, block.ndist);
    writeTrees(block.treecodes, block.ncode);
    writeTokens(block.tokens);
    break;
  }
}
void PreflateBlockReencoder::writeBits(const unsigned value, const unsigned bits) {
  bitbuffer |= (value & ((1 << bits) - 1)) << bitbuffersize;
  bitbuffersize += bits;
  while (bitbuffersize >= 8) {
    buffer[bufferpos++] = bitbuffer & 0xff;
    bitbuffer >>= 8;
    bitbuffersize -= 8;
    if (bufferpos >= BUFSIZE) {
      output.insert(output.end(), buffer, buffer + BUFSIZE);
      bufferpos = 0;
    }
  }
}
void PreflateBlockReencoder::flush() {
  writeBits(0, (8 - bitbuffersize) & 7);
  output.insert(output.end(), buffer, buffer + bufferpos);
  bufferpos = 0;
}

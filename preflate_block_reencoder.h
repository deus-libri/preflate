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

#ifndef PREFLATE_BLOCK_REENCODER_H
#define PREFLATE_BLOCK_REENCODER_H

#include "preflate_constants.h"
#include "preflate_input.h"
#include "preflate_token.h"

struct PreflateBlockReencoder {
  enum {
    BUFSIZE = 1024
  };

  std::vector<unsigned char> output;
  unsigned char buffer[BUFSIZE];
  unsigned bufferpos;
  unsigned bitbuffer;
  unsigned bitbuffersize;
  PreflateInput input;

  unsigned short litLenDistCodeStorage[PreflateConstants::LD_CODES];
  unsigned short treeCodeStorage[PreflateConstants::BL_CODES];
  unsigned char litLenDistBitStorage[PreflateConstants::LD_CODES];
  unsigned char treeBitStorage[PreflateConstants::BL_CODES];
  const unsigned short *litLenCode, *distCode, *treeCode;
  const unsigned char *litLenBits, *distBits, *treeBits;

  PreflateBlockReencoder(const std::vector<unsigned char>& dump);
  void writeBlock(const PreflateTokenBlock&, const bool last);
  void writeBits(unsigned value, unsigned bits);
  void writeTokens(const std::vector<PreflateToken>& tokens);
  void unpackTreeCodes(const std::vector<unsigned char>& treecodes, const unsigned ncode, const unsigned nlen);
  void generateTreeBitCodes(
    unsigned short* codes,
    const unsigned char* lengths,
    const unsigned size);
  void generateAllTreeBitCodes(const unsigned nlen, const unsigned ndist);
  void writeTrees(const std::vector<unsigned char>& treecodes, const unsigned count);
  void flush();
};

#endif /* PREFLATE_BLOCK_REENCODER_H */

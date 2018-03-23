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

#include "preflate_complevel_estimator.h"
#include "preflate_constants.h"
#include <algorithm>

PreflateCompLevelEstimatorState::PreflateCompLevelEstimatorState(
    const int wbits,
    const int mbits,
    const std::vector<unsigned char>& unpacked_output_,
    const std::vector<PreflateTokenBlock>& blocks_)
  : blocks(blocks_)
  , predictor(slowHash, slowPreflateParserSettings[5], wbits, mbits)
  , slowHash(unpacked_output_, mbits)
  , fastL1Hash(unpacked_output_, mbits)
  , fastL2Hash(unpacked_output_, mbits)
  , fastL3Hash(unpacked_output_, mbits)
{
  memset(&info, 0, sizeof(info));
  info.possibleCompressionLevels = (1 << 10) - (1 << 1);
}

void PreflateCompLevelEstimatorState::updateHash(const unsigned len) {
  if (info.possibleCompressionLevels & (1 << 1)) {
    fastL1Hash.updateHash(len);
  }
  if (info.possibleCompressionLevels & (1 << 2)) {
    fastL2Hash.updateHash(len);
  }
  if (info.possibleCompressionLevels & (1 << 3)) {
    fastL3Hash.updateHash(len);
  }
  slowHash.updateHash(len);
}
void PreflateCompLevelEstimatorState::updateOrSkipSingleFastHash(
    PreflateHashChainExt& hash,
    const unsigned len,
    const PreflateParserConfig& config) {
  if (len <= config.max_lazy) {
    hash.updateHash(len);
  } else {
    hash.skipHash(len);
  }
}

void PreflateCompLevelEstimatorState::updateOrSkipHash(const unsigned len) {
  if (info.possibleCompressionLevels & (1 << 1)) {
    updateOrSkipSingleFastHash(fastL1Hash, len, fastPreflateParserSettings[0]);
  }
  if (info.possibleCompressionLevels & (1 << 2)) {
    updateOrSkipSingleFastHash(fastL2Hash, len, fastPreflateParserSettings[1]);
  }
  if (info.possibleCompressionLevels & (1 << 3)) {
    updateOrSkipSingleFastHash(fastL3Hash, len, fastPreflateParserSettings[2]);
  }
  slowHash.updateHash(len);
}
bool PreflateCompLevelEstimatorState::checkMatchSingleFastHash(
    const PreflateToken& token,
    const PreflateHashChainExt& hash, 
    const PreflateParserConfig& config,
    const unsigned hashHead) {
  unsigned mdepth = predictor.matchDepth(hash.getHead(hashHead), token, hash);
  if (mdepth > config.max_chain) {
    return false;
  }
  return true;
}
void PreflateCompLevelEstimatorState::checkMatch(const PreflateToken& token) {
  unsigned hashHead = slowHash.curHash();
  if (info.possibleCompressionLevels & (1 << 1)) {
    if (!checkMatchSingleFastHash(token, fastL1Hash, fastPreflateParserSettings[0], hashHead)) {
      info.possibleCompressionLevels &= ~(1 << 1);
    }
  }
  if (info.possibleCompressionLevels & (1 << 2)) {
    if (!checkMatchSingleFastHash(token, fastL2Hash, fastPreflateParserSettings[1], hashHead)) {
      info.possibleCompressionLevels &= ~(1 << 2);
    }
  }
  if (info.possibleCompressionLevels & (1 << 3)) {
    if (!checkMatchSingleFastHash(token, fastL3Hash, fastPreflateParserSettings[2], hashHead)) {
      info.possibleCompressionLevels &= ~(1 << 3);
    }
  }

  info.referenceCount++;

  unsigned short mdepth = predictor.matchDepth(slowHash.getHead(hashHead), token, slowHash);
  if (mdepth >= 0x8001) {
    info.unfoundReferences++;
  } else {
    info.maxChainDepth = std::max(info.maxChainDepth, mdepth);
  }
  if (token.dist == predictor.currentInputPos()) {
    info.matchToStart = true;
  }
  if (mdepth == 0) {
    info.longestDistAtHop0 = std::max(info.longestDistAtHop0, token.dist);
  } else {
    info.longestDistAtHop1Plus = std::max(info.longestDistAtHop1Plus, token.dist);
  }
  if (token.len == 3) {
    info.longestLen3Dist = std::max(info.longestLen3Dist, token.dist);
  }
  if (info.possibleCompressionLevels & ((1 << 10) - (1 << 4))) {
    for (unsigned i = 0; i < 6; ++i) {
      if (!(info.possibleCompressionLevels & (1 << (4 + i)))) {
        continue;
      }
      const PreflateParserConfig& config = slowPreflateParserSettings[i];
      if (mdepth > config.max_chain) {
        info.possibleCompressionLevels &= ~(1 << (4 + i));
      }
    }
  }
}

void PreflateCompLevelEstimatorState::checkDump(bool early_out) {
  for (unsigned i = 0, n = blocks.size(); i < n; ++i) {
    const PreflateTokenBlock& b = blocks[i];
    if (b.type == PreflateTokenBlock::STORED) {
      updateHash(b.uncompressedLen);
      continue;
    }
    for (unsigned j = 0, m = b.tokens.size(); j < m; ++j) {
      const PreflateToken& t = b.tokens[j];
      if (t.len == 1) {
        updateHash(1);
      } else {
        checkMatch(t);
        updateOrSkipHash(t.len);
      }
      if (early_out && (info.possibleCompressionLevels & (info.possibleCompressionLevels - 1)) == 0) {
        return;
      }
    }
  }
}
void PreflateCompLevelEstimatorState::recommend() {
  info.recommendedCompressionLevel = 9;
  info.veryFarMatches = !(info.longestDistAtHop0 <= predictor.windowSize() - PreflateConstants::MIN_LOOKAHEAD
                          && info.longestDistAtHop1Plus < predictor.windowSize() - PreflateConstants::MIN_LOOKAHEAD);
  info.farLen3Matches = info.longestLen3Dist > 4096;

  info.zlibCompatible = info.possibleCompressionLevels > 1
                        && !info.matchToStart
                        && !info.veryFarMatches
                        && (!info.farLen3Matches || (info.possibleCompressionLevels & 0xe) != 0);
  if (info.unfoundReferences) {
    return;
  }

  if (info.zlibCompatible && info.possibleCompressionLevels > 1) {
    unsigned l = info.possibleCompressionLevels >> 1;
    info.recommendedCompressionLevel = 1;
    while ((l & 1) == 0) {
      info.recommendedCompressionLevel++;
      l >>= 1;
    }
    return;
  }
  for (int i = 0; i < 6; ++i) {
    const PreflateParserConfig& config = slowPreflateParserSettings[i];
    if (info.maxChainDepth <= config.max_chain) {
      info.recommendedCompressionLevel = 4 + i;
      return;
    }
  }
}

PreflateCompLevelInfo estimatePreflateCompLevel(
    const int wbits, 
    const int mbits,
    const std::vector<unsigned char>& unpacked_output,
    const std::vector<PreflateTokenBlock>& blocks,
    const bool early_out) {
  PreflateCompLevelEstimatorState state(wbits, mbits, unpacked_output, blocks);
  state.checkDump(early_out);
  state.recommend();
  return state.info;
}

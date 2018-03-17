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

#include "preflate_block_decoder.h"
#include "preflate_block_reencoder.h"
#include "preflate_checker.h"
#include "preflate_parameter_estimator.h"
#include "preflate_statistical_model.h"
#include "preflate_token_predictor.h"
#include "preflate_tree_predictor.h"
#include "preflate_unpack.h"
#include "support/bitstream.h"
#include "support/memstream.h"
#include "support/outputcachestream.h"

#include <algorithm>

bool preflate_checker(const std::vector<unsigned char>& deflate_raw) {
  printf("Checking raw deflate file of size %d\n", deflate_raw.size());

  std::vector<unsigned char> unpacked_output;
  std::vector<PreflateTokenBlock> blocks;
  if (!preflate_unpack(unpacked_output, blocks, deflate_raw)) {
    return false;
  }
  printf("Unpacked data has size %d\n", unpacked_output.size());
  MemStream decIn(deflate_raw);
  MemStream decUnc;
  BitInputStream decInBits(decIn);
  OutputCacheStream decOutCache(decUnc);
  std::vector<PreflateTokenBlock> blocks2;
  PreflateBlockDecoder bdec(decInBits, decOutCache);
  if (bdec.status() != PreflateBlockDecoder::OK) {
    return false;
  }
  bool last;
  unsigned i = 0;
  do {
    PreflateTokenBlock newBlock;
    bool ok = bdec.readBlock(newBlock, last);
    if (!ok) {
      return false;
    }
    if ((last && i + 1 != blocks.size())
      || (!last && i + 1 == blocks.size())) {
      return false;
    }
    if (!isEqual(newBlock, blocks[i])) {
      return false;
    }
    blocks2.push_back(newBlock);
    ++i;
  } while (!last);
  decOutCache.flush();
  if (decUnc.data() != unpacked_output) {
    for (unsigned i = 0, n = std::min(decUnc.data().size(), unpacked_output.size()); i < n; ++i) {
      if (decUnc.data()[i] != unpacked_output[i]) {
        printf("xxx %d\n", i);
      }
    }
    return false;
  }

  // Encode
  PreflateParameters paramsE = estimatePreflateParameters(unpacked_output, blocks);
  PreflateStatisticalModel modelE;
  memset(&modelE, 0, sizeof(modelE));
  PreflateTokenPredictor tokenPredictorE(paramsE, unpacked_output);
  PreflateTreePredictor treePredictorE(unpacked_output);
  for (unsigned i = 0, n = blocks.size(); i < n; ++i) {
    tokenPredictorE.analyzeBlock(i, blocks[i]);
    if (tokenPredictorE.predictionFailure) {
      printf("block %d: compress failed token prediction\n", i);
      return false;
    }
    treePredictorE.analyzeBlock(i, blocks[i]);
    if (treePredictorE.predictionFailure) {
      printf("block %d: compress failed tree prediction\n", i);
      return false;
    }
    tokenPredictorE.updateModel(&modelE, i);
    treePredictorE.updateModel(&modelE, i);
  }

  modelE.print();

  PreflateStatisticalEncoder codecE(modelE);
  codecE.encodeHeader();
  codecE.encodeParameters(paramsE);
  codecE.encodeModel();
  for (unsigned i = 0, n = blocks.size(); i < n; ++i) {
    tokenPredictorE.encodeBlock(&codecE, i);
    if (tokenPredictorE.predictionFailure) {
      printf("block %d: compress failed token encoding\n", i);
      return false;
    }
    treePredictorE.encodeBlock(&codecE, i);
    if (treePredictorE.predictionFailure) {
      printf("block %d: compress failed tree encoding\n", i);
      return false;
    }
  }
  std::vector<unsigned char> preflate_diff = codecE.encodeFinish();
  printf("Prediction diff has size %d\n", preflate_diff.size());

  // Decode
  PreflateStatisticalDecoder codecD(preflate_diff);
  if (!codecD.decodeHeader()) {
    printf("header decoding failed\n");
    return false;
  }
  PreflateParameters paramsD;
  if (!codecD.decodeParameters(paramsD)) {
    printf("parameter decoding failed\n");
    return false;
  }
  if (paramsD.strategy != paramsE.strategy) {
    printf("parameter decoding failed: strategy mismatch\n");
    return false;
  }
  if (paramsD.huffStrategy != paramsE.huffStrategy) {
    printf("parameter decoding failed: huff strategy mismatch\n");
    return false;
  }
  if (paramsD.windowBits != paramsE.windowBits) {
    printf("parameter decoding failed: windowBits mismatch\n");
    return false;
  }
  if (paramsD.memLevel != paramsE.memLevel) {
    printf("parameter decoding failed: memLevel mismatch\n");
    return false;
  }
  if (paramsD.compLevel != paramsE.compLevel) {
    printf("parameter decoding failed: compLevel mismatch\n");
    return false;
  }
  if (paramsD.isCompLevelFallback != paramsE.isCompLevelFallback) {
    printf("parameter decoding failed: memLevel mismatch\n");
    return false;
  }

  if (!codecD.decodeModel()) {
    printf("model decoding failed\n");
    return false;
  }
  if (!isEqual(*codecD.model, *codecE.model)) {
    printf("decoded model differs from original\n");
    return false;
  }

  PreflateTokenPredictor tokenPredictorD(paramsD, unpacked_output);
  PreflateTreePredictor treePredictorD(unpacked_output);
  PreflateBlockReencoder deflater(unpacked_output);
  unsigned blockno = 0;
  do {
    if (blockno >= blocks.size()) {
      printf("block number too big: org %d, new %d\n", blocks.size(), blockno);
      return false;
    }
    PreflateTokenBlock block = tokenPredictorD.decodeBlock(&codecD);
    if (tokenPredictorD.predictionFailure) {
      printf("block %d: token uncompress failed\n", blockno);
      return false;
    }
    if (block.type != blocks[blockno].type) {
      printf("block %d: type differs: org %d, new %d\n", blockno, blocks[blockno].type, block.type);
      return false;
    }
    for (unsigned i = 0, n = std::min(block.tokens.size(), blocks[blockno].tokens.size()); i < n; ++i) {
      PreflateToken orgToken = blocks[blockno].tokens[i];
      PreflateToken newToken = block.tokens[i];
      if (newToken.len != orgToken.len || newToken.dist != orgToken.dist) {
        printf("block %d: generated token %d differs: org(%d,%d), new(%d,%d)\n",
               blockno, i, orgToken.len, orgToken.dist, newToken.len, newToken.dist);
        return false;
      }
    }
    if (block.tokens.size() != blocks[blockno].tokens.size()) {
      printf("block %d: differing token count: org %d, new %d\n",
             blockno, blocks[blockno].tokens.size(), block.tokens.size());
      return false;
    }

    if (!treePredictorD.decodeBlock(block, &codecD)) {
      printf("block %d: tree uncompress failed\n", blockno);
      return false;
    }
    if (treePredictorD.predictionFailure) {
      printf("block %d: tree uncompress failed\n", blockno);
      return false;
    }
    if (block.nlen != blocks[blockno].nlen) {
      printf("block %d: literal/len count differs: org %d, new %d\n", 
             blockno, blocks[blockno].nlen, block.nlen);
      return false;
    }
    if (block.ndist != blocks[blockno].ndist) {
      printf("block %d: dist count differs: org %d, new %d\n",
             blockno, blocks[blockno].ndist, block.ndist);
      return false;
    }
    if (block.ncode != blocks[blockno].ncode) {
      printf("block %d: tree code count differs: org %d, new %d\n",
             blockno, blocks[blockno].ncode, block.ncode);
      return false;
    }
    if (block.treecodes != blocks[blockno].treecodes) {
      printf("block %d: generated tree codes differs\n", blockno);
      return false;
    }
    deflater.writeBlock(block, tokenPredictorD.eof());
    ++blockno;
  } while (!tokenPredictorD.eof());
  deflater.flush();

  std::vector<unsigned char> deflate_raw_out = std::move(deflater.output);
  for (unsigned i = 0, n = std::min(deflate_raw.size(), deflate_raw_out.size()); i < n; ++i) {
    if (deflate_raw[i] != deflate_raw_out[i]) {
      printf("created deflate stream differs at offset %d\n", i);
      return false;
    }
  }
  if (deflate_raw.size() != deflate_raw_out.size()) {
    printf("created deflate streams differs in size: org %d, new %d\n", deflate_raw.size(), deflate_raw_out.size());
    return false;
  }
  printf("Success\n");
  return true;
}

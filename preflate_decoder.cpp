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

#include "preflate_decoder.h"
#include "preflate_parameter_estimator.h"
#include "preflate_statistical_model.h"
#include "preflate_token_predictor.h"
#include "preflate_tree_predictor.h"
#include "preflate_unpack.h"

bool preflate_decode(std::vector<unsigned char>& unpacked_output,
                     std::vector<unsigned char>& preflate_diff,
                     const std::vector<unsigned char>& deflate_raw) {
  std::vector<PreflateTokenBlock> blocks;
  if (!preflate_unpack(unpacked_output, blocks, deflate_raw)) {
    return false;
  }

  PreflateParameters params = estimatePreflateParameters(unpacked_output, blocks);
  PreflateStatisticalModel model;
  memset(&model, 0, sizeof(model));
  PreflateTokenPredictor tokenPredictor(params, unpacked_output);
  PreflateTreePredictor treePredictor(unpacked_output);
  for (unsigned i = 0, n = blocks.size(); i < n; ++i) {
    tokenPredictor.analyzeBlock(i, blocks[i]);
    treePredictor.analyzeBlock(i, blocks[i]);
//    printf("Block %d", i);
    if (tokenPredictor.predictionFailure || treePredictor.predictionFailure) {
      return false;
    }
    tokenPredictor.updateModel(&model, i);
    treePredictor.updateModel(&model, i);
//    model.print();
  }
  PreflateStatisticalEncoder codec(model);
  codec.encodeHeader();
  codec.encodeParameters(params);
  codec.encodeModel();
//    printf("model size %d\n", codec.data->getpos());
  for (unsigned i = 0, n = blocks.size(); i < n; ++i) {
//      unsigned sz0 = codec.data->getpos();
    tokenPredictor.encodeBlock(&codec, i);
//      unsigned sz1 = codec.data->getpos();
    treePredictor.encodeBlock(&codec, i);
    if (tokenPredictor.predictionFailure || treePredictor.predictionFailure) {
      return false;
    }
//      printf("block: %d, tokens %d -> token sz %d, tree sz %d\n",
//              i,
//              dumper->getBlocks()[i]->tokens.size(),
//              sz1 - sz0,
//              codec.data->getpos() - sz1);
  }
  preflate_diff = codec.encodeFinish();
  return true;
}

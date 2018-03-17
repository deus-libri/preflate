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
#include <stdio.h>
#include <stdlib.h>

#include "preflate_info.h"
#include "preflate_checker.h"
#include "support/support_tests.h"

bool loadfile(
    std::vector<unsigned char>& content, 
    const std::string& fn) {
  FILE* f = fopen(fn.c_str(), "rb");
  if (!f) {
    return false;
  }
  fseek(f, 0, SEEK_END);
  long length = ftell(f);
  fseek(f, 0, SEEK_SET);
  content.resize(length);
  long read = fread(content.data(), 1, content.size(), f);
  fclose(f);
  return read == length;
}

int main(int argc, char **argv) {
  if (!support_self_tests()) {
    return -1;
  }

/*  static const char* const def_fns[] = {
    "../testdata/zlib1.raw", "../testdata/zlib2.raw", "../testdata/zlib3.raw", 
    "../testdata/zlib4.raw", "../testdata/zlib5.raw", "../testdata/zlib6.raw", 
    "../testdata/zlib7.raw", "../testdata/zlib9.raw",
    "../testdata/7zfast.raw", "../testdata/7zultra.raw", "../testdata/kz.raw",
    "../testdata/dump.raw", "../testdata/dump_kz.raw",
    };
    */
static const char* const def_fns[] = {
  "../testdata/zlib1.raw", "../testdata/zlib2.raw", "../testdata/zlib3.raw",
  "../testdata/zlib4.raw", "../testdata/zlib5.raw", "../testdata/zlib6.raw",
  "../testdata/zlib7.raw", "../testdata/zlib9.raw",
  "../testdata/7zfast.raw", "../testdata/7zultra.raw", "../testdata/kz.raw",
//  "../testdata/dump.raw", "../testdata/dump_kz.raw",
};
  const char* const * fns = def_fns;
  unsigned fncnt = sizeof(def_fns) / sizeof(def_fns[0]);

  if (argc >= 2) {
    fns   = argv + 1;
    fncnt = argc - 1;
  }
  bool ok = true;
  for (unsigned i = 0; i < fncnt; ++i) {
    std::vector<unsigned char> content;
    if (!loadfile(content, fns[i])) {
      printf("loading of %s failed\n", fns[i]);
      ok = false;
    } else {
      printf("checking %s\n", fns[i]);
      bool check_ok = preflate_checker(content);
      ok = ok && check_ok;
    }
  }
  if (ok) {
    printf("All checks ok\n");
  }
  return ok ? 0 : -1;
}

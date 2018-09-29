preflate v0.3.5
===============
Library to split deflate streams into uncompressed data and reconstruction information,
or reconstruct the original deflate stream from those two. 


What's it all about?
--------------------
Long term storage. There are a lot of documents, archives, etc that contain deflate stream.
Deflate is a very simple, but not very strong compression algorithm. For long term storage,
it makes sense to compress the uncompressed deflate stream with a stronger algorithm, like
lzma, rolz, bwt or others.

Reconstructing the original deflate stream becomes important if the position or size
of the reconstructed deflate streams must not differ, e.g. if those streams are embedded
into executables, or unsupported archive files where the indices cannot be adapted correctly.

There are currently already some other tools available which try to solve this problem:
- "precomp" is a tool which used to be able to do the bit-correct reconstruction very efficiently,
  but only for deflate streams that were created by the ZLIB library. (It only needed
  to store the three relevant ZLIB parameters to allow reconstruction.)
  It bailed out for anything created by 7zip, kzip, or similar tools.
  Of course, "precomp" also handles JPG, PNG, ZIP, GIF, PDF, MP3, etc, which makes
  a very nice tool, and it is open source.
  *UPDATE* The latest development version of "precomp" incorporates the "preflate" library.
- "reflate" can reconstruct any deflate stream (also 7zip, kzip, etc), but it is only 
  close to perfect for streams that were created by ZLIB, compression level 9. 
  All lower compression levels of ZLIB require increasing reconstruction info, the further
  from level 9, the bigger the required reconstruction data.
  "reflate" only handles deflate streams, and is not open source. As far as I know,
  it is also part of PowerArchiver.
- "grittibanzli" is also open source, and can reconstruct any deflate stream. 
  (It was published after the first version of "preflate".)
  It outputs the reconstruction data (the diff against the prediction) in 
  an uncompressed, byte based format, and relies on an external compressor to squeeze it
  down. This work quite well, and in my tests, the compressed reconstruction data is 
  usually only 20 to 30% bigger than "preflate"s, and smaller than "reflate"s.
  The ZLIB level detection is rather basic (worse then "preflate"s, and much worse than
  "precomp"s), and for streams which were compressed with a lower level of ZLIB, it
  will create reconstruction data that are not just a few bytes.
  In my tests, compression time was the same for "grittibanzli" and "preflate", while
  for decompression it was faster. However, the higher memory requirements will put
  a strain on some systems. I also got reports from other testers which did more intensive
  tests where "grittibanzli" was much slower than "preflate" and has occasional crashes.
  There doesn't seem to be active development right now.


So, what is the point of "preflate"?
------------------------------------
The goal of "preflate" is to get the best of both worlds:
- for deflate streams created by ZLIB at any compression level, we want to
  be able to reconstruct it with only a few bytes of reconstruction information (like "precomp")
- for all other deflate streams, we want to be able to reconstruct them with
  reconstruction information that is not much larger than "reflate"

Right now, it has been tested on some ten thousand valid deflate streams, some extracted with
"rawdet" from archives etc., and "preflate" was capable of inflating and reconstructing 
all of them. v0.3.2 was incorporated into a fork of "precomp" on Windows, and tested
with 100+GB of data without problems. "preflate" is now even part of mainline "precomp",
which caused more feedback and the v0.3.5 bug fix release.

There might be some unknown corner cases of valid deflate stream in which preflate 
will fail. And there are probably some cases of invalid deflate streams which will
make it crash.


So, is "preflate" already better than "precomp" and "reflate"? 
--------------------------------------------------------------
No. It isn't. However, it is a reasonable alternative.

First, "preflate" only works on raw deflate streams. So, it is not intended as 
a tool, but as a library to be used by some other tool. 
The ZLIB level detection inside previous versions of "precomp" were much better than
"preflate"s (and it needed to be), so there are a considerable amount of ZLIB deflate
streams, probably 20-30%, for which "preflate" generates reconstruction data that
is bigger than 3 bytes (usually only a few tens of bytes though).
Or in other words: there are files in which previous "precomp" version BEAT "preflate"
in SIZE.

It's quite slow. Since 0.2.1, there is some optimization for long runs of the same byte (e.g.
\0 or spaces), and some files profit from that tremendously (which were ten times
slower than now), but for most files the gain is only a few percent.
Since 0.3.3, preflate utilizes a task pool which gives a nice
speedup when handling large deflate streams on multi-core machines.
Single thread performance is still poor though.
Both previous "precomp" versions and "reflate" BEAT "preflate" in SPEED for small deflate
streams or when restricted to a single core. (Expected to be around
50-500%).

However, "preflate" eats anything and can be successfully applied to files in which
previous versions of "precomp" will fail completely. It generates smaller reconstruction data than
"reflate" (in my tests). And it is open source.


How do I build it?
------------------
There is a CMake script which was tested mostly on Windows with MSVC.

Note: Support for std::thread (used in preflate since 0.3.3) is tricky
in MinGW, and you might need particular MinGW builds for that to work.


Credits
-------
- "precomp" by Christian Schneider
  (https://github.com/schnaader/precomp-cpp)
- "reflate" and "rawdet" by Eugene Shelwien
- "zlib" by Mark Adler et al.
- "7zip" by Igor Pavlov
- "kzip" by Ken Silverman
- "grittibanzli" by Google Zurich (inofficial project)
  (https://github.com/google/grittibanzli)

All of the software above is just AWESOME!

- the ENCODE.RU data compression forum (http://encode.ru/forum/2-Data-Compression)

If you want to know about the new hot stuff in data compression (doesn't happen
too often though), look here first.

- www.squeezechart.com by Stephan Busch

Contains information about a lot of interesting compression tools of which I probably
would never have known without this site. Also, Stephan helped getting rid of several
bugs in preflate. Thank you.

- packARI by Matthias Stirner

packARI provides context-aware, adaptive order(n) encoding. "preflate"s
arithmetic coder is based on packARI's algorithms, but only uses static
models and contains some speed optimizations which are totally pointless because
"preflate" spends all its time in the match finder.


Changes
-------
- 0.1.0 - first public release
- 0.2.0 - freeze bitstream format, remove zlib and packARI dependencies
- 0.2.1 - add match finder for same character sequences
- 0.3.0 - split up large deflate streams into "meta-blocks" of a few megabyte each
        (this should help keep memory usage in check)
- 0.3.1 - mitigation against zlib level estimation failure due to 
        "meta block splitting". For one 350KiB file compressed at zlib level 1,
         reconstruction size goes from 35KiB to 1.5KiB. Without meta-blocks it
         would be 3 bytes...
- 0.3.2 - bug fix: preflate would occasionally believe that it could
          treat "abaaaa" as a sequence of a's, totally ignoring the b.
          Thanks to Gonzalo Munoz for finding this.
- 0.3.3 - add task pool to handle meta blocks in parallel. 
         (multi-threading is why I added meta blocks in the first place.)
         for small deflate streams consisting of only one meta block, the task pool is ignored.
- 0.3.4 - bug fix for 0.3.1: preflate would occasionally fail because the
          zlib parameter estimator would wrongly restrict its search range
          for large deflate streams split into meta blocks
- 0.3.5 - bug fix: some undefined behaviour could lead to crashes.
          Thanks to Andrew Epstein for reporting the bug and pointing to clang's sanitizers
          to spot out-of-bounds accesses and undefined behaviour.
          Also, 0.3.3 introduced a crash bug in the context of large invalid deflate streams.
          Thanks to Christian Schneider for reporting.

License
-------
Copyright 2018 Dirk Steinke

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

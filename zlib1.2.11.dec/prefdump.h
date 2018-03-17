#ifndef PREF_DUMP_H
#define PREF_DUMP_H

#    include <stddef.h>
#include "zconf.h"
#include "zutil.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int ZLIB_INTERNAL prefdump_valid OF((voidpf dumper)) { return dumper != 0L; }

void ZLIB_INTERNAL prefdump_stream_header OF((voidpf dumper, int wlen, int zhdr2));
void ZLIB_INTERNAL prefdump_dictid OF((voidpf dumper, uLong dictid));
void ZLIB_INTERNAL prefdump_new_stored_block OF((voidpf dumper, int len));
void ZLIB_INTERNAL prefdump_new_huff_block OF((voidpf dumper, int dynamic));
void ZLIB_INTERNAL prefdump_dyn_huff_lengths OF((voidpf dumper, int nlen, int ndist, int ncode));
void ZLIB_INTERNAL prefdump_dyn_huff_tcode OF((voidpf dumper, int code));
void ZLIB_INTERNAL prefdump_literal OF((voidpf dumper));
void ZLIB_INTERNAL prefdump_reference OF((voidpf dumper, int dist, int len));
void ZLIB_INTERNAL prefdump_eob OF((voidpf dumper));

//int ZLIB_INTERNAL prefdump_get_stream_header_byte OF((voidpf dumper));

#ifdef __cplusplus
}
#endif

#endif 
// PREF_DUMP_H

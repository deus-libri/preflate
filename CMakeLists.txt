cmake_minimum_required(VERSION 3.2)

project(preflate)

set(SRCDIR ${CMAKE_CURRENT_SOURCE_DIR})
set(OBJDIR ${CMAKE_CURRENT_BINARY_DIR})

include_directories(AFTER ${SRCDIR})

function(add_stem2file VAR pattern stem)
    set(TMPV ${${VAR}})
    foreach(IND ${stem})
        string(REGEX REPLACE "^[ ]*([0-9a-zA-Z_%]+)[ ]*$" \\1 IND ${IND})
        string(REPLACE "%STEM%" ${IND} NEXT ${pattern})
        set(TMPV "${TMPV};${NEXT}")
    endforeach()
    set(${VAR} "${TMPV}" PARENT_SCOPE)
endfunction()

set(LIB_SRC "")
add_stem2file(LIB_SRC "${SRCDIR}/preflate_%STEM%.cpp" 
              "block_decoder;block_reencoder;block_trees;\
               complevel_estimator;constants;decoder;hash_chain;info;\
               parameter_estimator;parser_config;predictor_state;reencoder;\
               statistical_codec;statistical_model;token;\
               token_predictor;tree_predictor")
add_stem2file(LIB_SRC "${SRCDIR}/support/%STEM%.cpp"
              "arithmetic_coder;array_helper;bit_helper;bitstream;const_division;\
               filestream;huffman_decoder;huffman_encoder;huffman_helper;memstream;\
               outputcachestream;task_pool")
add_stem2file(LIB_SRC "${SRCDIR}/packARI/source/%STEM%.cpp" 
              "aricoder;bitops")

set(DEMO_SRC "")
add_stem2file(DEMO_SRC "${SRCDIR}/preflate_%STEM%.cpp" 
              "checker;dumper;unpack")
add_stem2file(DEMO_SRC "${SRCDIR}/support/%STEM%.cpp"
              "support_tests")
add_stem2file(DEMO_SRC "${SRCDIR}/zlib1.2.11.dec/%STEM%.c"
              "adler32;inffast;inflate;inftrees;trees;zutil")

set(LIB_HDR "")
add_stem2file(LIB_HDR "${SRCDIR}/preflate_%STEM%.h" 
              "block_decoder;block_reencoder;block_trees;checker;\
               complevel_estimator;constants;decoder;dumper;hash_chain;info;\
               parameter_estimator;parser_config;predictor_state;reencoder;\
               statistical_codec;statistical_model;token;\
               token_predictor;tree_predictor;unpack")
add_stem2file(LIB_HDR "${SRCDIR}/support/%STEM%.h" 
              "arithmetic_coder;array_helper;bitstream;bit_helper;const_division;\
               filestream;huffman_decoder;huffman_encoder;huffman_helper;\
               memstream;outputcachestream;support_tests;task_pool")
add_stem2file(LIB_HDR "${SRCDIR}/packARI/source/%STEM%.h" 
              "aricoder;bitops")

add_definitions(-DZ_SOLO -DNO_GZIP)
set(DEMO_HDR "${SRCDIR}/main.cpp")
add_stem2file(DEMO_HDR "${SRCDIR}/preflate_%STEM%.h" 
              "checker;dumper;unpack")
add_stem2file(DEMO_HDR "${SRCDIR}/support/%STEM%.h" 
              "support_tests")
add_stem2file(DEMO_HDR "${SRCDIR}/zlib1.2.11.dec/%STEM%.h" 
              "inffast;inffixed;inflate;inftrees;trees;zconf;zlib;zutil")
               
add_library(preflate STATIC ${LIB_SRC} ${LIB_HDR})
add_executable(preflate_demo ${DEMO_SRC} ${DEMO_HDR})
target_link_libraries(preflate_demo preflate)

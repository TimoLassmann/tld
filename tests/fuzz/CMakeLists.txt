# libFuzzer targets for everything that reads outside input: JSON, templates, and the
# replies tld_ai.h reads from a server (decoding and the full HTTP exchange against a
# fake server). Needs clang; configure with -DTLD_FUZZ=ON -DCMAKE_C_COMPILER=clang and
# run with tests/fuzz/run.sh SECONDS BUILD_DIR. ASan and UBSan are always on.
find_package(Threads REQUIRED)
foreach(t json template ai http)
  add_executable(fuzz-${t} fuzz_${t}.c)
  target_include_directories(fuzz-${t} PRIVATE ${CMAKE_SOURCE_DIR})
  set_target_properties(fuzz-${t} PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
  target_compile_definitions(fuzz-${t} PRIVATE _GNU_SOURCE)
  target_compile_options(fuzz-${t} PRIVATE -g -O1 -fsanitize=fuzzer,address,undefined
                         -fno-sanitize-recover=undefined)
  target_link_options(fuzz-${t} PRIVATE -fsanitize=fuzzer,address,undefined)
  target_link_libraries(fuzz-${t} PRIVATE m Threads::Threads)
endforeach()

# tld_ai.h tests (PRD 6.9/6.10). Programs are built from ./tld_ai.h and ./tld.h only, with the
# same compile flags as everything else in this build; tests/ai/run.sh drives them
# against tests/ai/mock_ollama.py. No test ever talks to a real Ollama server.
find_package(Threads REQUIRED)

function(ai_program name src)
  add_executable(${name} ${src})
  target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/ai)
  set_target_properties(${name} PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
  target_link_libraries(${name} PRIVATE m Threads::Threads)
endfunction()

ai_program(ai-triage ${CMAKE_SOURCE_DIR}/examples/triage.c)
ai_program(ai-par ${CMAKE_SOURCE_DIR}/examples/par.c)
ai_program(ai-hello ${CMAKE_SOURCE_DIR}/examples/hello.c)
ai_program(ai-opts ai/opts.c)
ai_program(ai-text ai/text.c)
ai_program(ai-apis ai/apis.c)
ai_program(ai-special ai/special.c)
ai_program(ai-defaults ai/defaults.c)
ai_program(ai-arena ai/arena.c)
add_test(NAME ai-arena COMMAND ai-arena)
ai_program(oom-ai ${CMAKE_CURRENT_SOURCE_DIR}/oom/oom_ai.c)

set(AI_RUN sh ${CMAKE_CURRENT_SOURCE_DIR}/ai/run.sh)
foreach(group plumbing noserver http json cache defaults oom)
  add_test(NAME ai-${group}
    COMMAND ${AI_RUN} ${group} $<TARGET_FILE_DIR:ai-triage>
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(ai-${group} PROPERTIES TIMEOUT 300)
endforeach()

add_test(NAME ai-compile
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/ai/compile.sh ${CMAKE_C_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ASan + UBSan: in an ASAN build the programs above already carry the sanitizers
# (configure with -DCMAKE_BUILD_TYPE=ASAN -DCMAKE_C_FLAGS="-fsanitize=undefined
# -fno-sanitize-recover=undefined"); this test also fails on any sanitizer report.
if(CMAKE_BUILD_TYPE MATCHES ASAN)
  add_test(NAME ai-sanitize
    COMMAND ${AI_RUN} sanitize $<TARGET_FILE_DIR:ai-triage>
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(ai-sanitize PROPERTIES TIMEOUT 600)
else()
  # TSan cannot be combined with ASan, so it gets its own targets in other builds.
  include(CheckCSourceCompiles)
  set(CMAKE_REQUIRED_FLAGS "-fsanitize=thread")
  set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=thread")
  check_c_source_compiles("int main(void){return 0;}" AI_HAVE_TSAN)
  unset(CMAKE_REQUIRED_FLAGS)
  unset(CMAKE_REQUIRED_LINK_OPTIONS)
  if(AI_HAVE_TSAN)
    foreach(p par defaults)
      if(p STREQUAL par)
        ai_program(ai-${p}-tsan ${CMAKE_SOURCE_DIR}/examples/par.c)
      else()
        ai_program(ai-${p}-tsan ai/defaults.c)
      endif()
      target_compile_options(ai-${p}-tsan PRIVATE -fsanitize=thread -g -O1)
      target_link_options(ai-${p}-tsan PRIVATE -fsanitize=thread)
    endforeach()
    add_test(NAME ai-tsan
      COMMAND ${AI_RUN} tsan $<TARGET_FILE_DIR:ai-triage>
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(ai-tsan PROPERTIES TIMEOUT 600)
  endif()
endif()

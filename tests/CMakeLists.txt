cmake_minimum_required(VERSION 3.18)

project(tld-tests LANGUAGES C)

# Layout (see tests/README.md):
#   unit/         unit-<module>      one program per module; unit-doc for tld_doc.h
#   regression/   regress-<name>     tests written when a bug was fixed
#   integration/  int-*, single-header*   whole-library and packaging tests
#   ai/           ai-*               tld_ai.h against a mock Ollama (ai.cmake)
#   doc/          doc-eval           tld_doc.h on the evaluation set
#   oom/          oom-tld, oom-ai    every allocation fails in turn (oom-tld covers tld_doc.h; oom-ai runs in ai.cmake)
#   null/         null-args, null-args-doc   every public function with NULL pointer arguments
# Every program includes tests/test.h (-h/-v) and with no arguments runs its checks.

find_program(MEMORYCHECK_COMMAND valgrind NO_CACHE)
if (MEMORYCHECK_COMMAND)
  message(STATUS "Found Valgrind: ${MEMORYCHECK_COMMAND}")
  set(MEMORYCHECK_COMMAND_OPTIONS
    "--leak-check=full"
    "--show-leak-kinds=all"
    "--error-exitcode=1"
  )
else()
  message(STATUS "Valgrind not found")
  set(MEMORYCHECK_COMMAND "")
  set(MEMORYCHECK_COMMAND_OPTIONS "")
endif()

find_package(Threads REQUIRED)

# tld_test(NAME SOURCE...): a program linked against tld-dev, run by ctest (under
# valgrind when it is installed).
function(tld_test TEST_NAME)
  add_executable(${TEST_NAME} ${ARGN})
  target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${TEST_NAME} PRIVATE tld-dev)
  add_test(
    NAME ${TEST_NAME}
    COMMAND ${MEMORYCHECK_COMMAND} ${MEMORYCHECK_COMMAND_OPTIONS} $<TARGET_FILE:${TEST_NAME}>
  )
endfunction()

# unit/: unit-<module> from unit/unit_<module>.c
foreach(m align alloc auc bitfield bpm core hash json kdtree logsum misc pearson
          qnorm rng shuffle sigmoid sort stats str template timing)
  tld_test(unit-${m} unit/unit_${m}.c)
endforeach()
# unit-tpool uses threads and is not run under valgrind.
add_executable(unit-tpool unit/unit_tpool.c)
target_include_directories(unit-tpool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(unit-tpool PRIVATE tld-dev Threads::Threads)
add_test(NAME unit-tpool COMMAND unit-tpool)

# unit-bpm-wave and unit-bpm-band: unit-bpm built from ./tld.h with the internal
# thresholds changed (the SIMD wavefront from 2 words on; the cut-off always; short
# stretches for the four-at-once scan), so every path is checked against the reference.
foreach(v wave band)
  add_executable(unit-bpm-${v} unit/unit_bpm_${v}.c)
  target_include_directories(unit-bpm-${v} PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
  set_target_properties(unit-bpm-${v} PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
  target_link_libraries(unit-bpm-${v} PRIVATE m Threads::Threads)
  add_test(NAME unit-bpm-${v} COMMAND ${MEMORYCHECK_COMMAND} ${MEMORYCHECK_COMMAND_OPTIONS} $<TARGET_FILE:unit-bpm-${v}>)
endforeach()

# unit-doc: tld_doc.h, built from ./tld_doc.h and ./tld.h; checks the corpus
# against docs/tld.md, docs/tld_ai.md and the headers of this source tree.
add_executable(unit-doc unit/unit_doc.c)
target_include_directories(unit-doc PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(unit-doc PRIVATE TLD_DOC_TEST_ROOT="${CMAKE_SOURCE_DIR}")
set_target_properties(unit-doc PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
target_link_libraries(unit-doc PRIVATE m Threads::Threads)
add_test(NAME unit-doc COMMAND ${MEMORYCHECK_COMMAND} ${MEMORYCHECK_COMMAND_OPTIONS} $<TARGET_FILE:unit-doc>)

# doc-eval: tld_doc.h over the evaluation set doc/questions.tsv: prints the retrieval
# figures and fails on a miss that the set does not mark as known.
add_executable(doc-eval doc/eval.c)
target_include_directories(doc-eval PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(doc-eval PRIVATE TLD_DOC_TEST_ROOT="${CMAKE_SOURCE_DIR}")
set_target_properties(doc-eval PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
target_link_libraries(doc-eval PRIVATE m Threads::Threads)
add_test(NAME doc-eval COMMAND doc-eval)
# doc-eval-unknown-name: doc-eval fails when an expected name is not in the corpus.
add_test(NAME doc-eval-unknown-name COMMAND doc-eval ${CMAKE_CURRENT_SOURCE_DIR}/doc/unknown_name.tsv)
set_tests_properties(doc-eval-unknown-name PROPERTIES
  PASS_REGULAR_EXPRESSION "Question u01: expected tld_json_dump_NO_SUCH_NAME is not a name in the corpus")
# examples/tld_ask.c, the command line front end of tld_doc.h; tld-ask-run checks that
# it answers.
add_executable(tld-ask ${CMAKE_SOURCE_DIR}/examples/tld_ask.c)
target_include_directories(tld-ask PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(tld-ask PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
target_link_libraries(tld-ask PRIVATE m Threads::Threads)
add_test(NAME tld-ask-run COMMAND tld-ask "how do I read a JSON string?")
set_tests_properties(tld-ask-run PROPERTIES PASS_REGULAR_EXPRESSION "\nENTRY 1: ")

# regression/: regress-<name> from regression/regress_<name>.c
foreach(r alloc hash json json_build kdtree logsum misc msg rng shuffle sort_cmp
          stats str template)
  string(REPLACE "_" "-" n ${r})
  tld_test(regress-${n} regression/regress_${r}.c)
endforeach()

# integration/
tld_test(int-kde integration/kde.c)
tld_test(int-noise integration/noise.c)

# Single-header build: ./tld.h (generated by tools/amalgamate.py) used without tld-dev,
# in two translation units so duplicate or missing symbols fail to link.
add_executable(single-header integration/single_impl.c integration/single_use.c)
target_include_directories(single-header PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(single-header PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
target_link_libraries(single-header PRIVATE m Threads::Threads)
add_test(NAME single-header COMMAND single-header)
# The same two TUs in strict ISO C with warnings as errors: on glibc this caught
# realpath/localtime_r/strnlen being used without declarations.
add_test(NAME single-header-strict
  COMMAND ${CMAKE_C_COMPILER} -std=c11 -Wall -Wextra -pedantic -Werror
          -I${CMAKE_SOURCE_DIR} -I${CMAKE_CURRENT_SOURCE_DIR}
          ${CMAKE_CURRENT_SOURCE_DIR}/integration/single_impl.c
          ${CMAKE_CURRENT_SOURCE_DIR}/integration/single_use.c
          -o ${CMAKE_CURRENT_BINARY_DIR}/single-header-strict -lm -lpthread)
# The same for a program using the single headers ./tld_doc.h and ./tld.h: built with
# the build's flags and run, and built in strict ISO C with warnings as errors.
add_executable(single-header-doc integration/single_doc_impl.c integration/single_doc_use.c)
target_include_directories(single-header-doc PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(single-header-doc PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
target_link_libraries(single-header-doc PRIVATE m Threads::Threads)
add_test(NAME single-header-doc COMMAND single-header-doc)
add_test(NAME single-header-doc-strict
  COMMAND ${CMAKE_C_COMPILER} -std=c11 -Wall -Wextra -pedantic -Werror
          -I${CMAKE_SOURCE_DIR} -I${CMAKE_CURRENT_SOURCE_DIR}
          ${CMAKE_CURRENT_SOURCE_DIR}/integration/single_doc_impl.c
          ${CMAKE_CURRENT_SOURCE_DIR}/integration/single_doc_use.c
          -o ${CMAKE_CURRENT_BINARY_DIR}/single-header-doc-strict -lm -lpthread)

# ./tld.h must be regenerated whenever src/ changes.
find_program(UV_PROGRAM uv)
if(UV_PROGRAM)
  add_test(NAME single-header-current
    COMMAND ${UV_PROGRAM} run python ${CMAKE_SOURCE_DIR}/tools/amalgamate.py --check
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND)
    add_test(NAME single-header-current
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/amalgamate.py --check
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  endif()
endif()

# migrate/: tools/migrate.py turns old.c (code for the old tld) into expected.c, changes
# nothing on a second run, and flags what needs a person.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME migrate
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/migrate/run.sh ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/migrate)
endif()

# oom/: every allocation of each scenario fails in turn (built from ./tld.h; leaks, crashes
# and failures reported as success fail the test). Writes oom-NAME.err here on failure.
add_executable(oom-tld oom/oom_tld.c)
target_include_directories(oom-tld PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(oom-tld PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
target_link_libraries(oom-tld PRIVATE m Threads::Threads)
add_test(NAME oom-tld COMMAND oom-tld WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(oom-tld PROPERTIES TIMEOUT 600)

# Every public function with NULL in each pointer argument: the failure value and an
# ERROR line (null/null_args.c); covered.sh fails when a function has no case.
tld_test(null-args null/null_args.c)
# null-args-doc: the same program for tld_doc.h (its DOC_CASES), built from ./tld_doc.h
# and ./tld.h.
add_executable(null-args-doc null/null_args.c)
target_include_directories(null-args-doc PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(null-args-doc PRIVATE TLD_NULL_DOC=1)
set_target_properties(null-args-doc PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
target_link_libraries(null-args-doc PRIVATE m Threads::Threads)
add_test(NAME null-args-doc COMMAND ${MEMORYCHECK_COMMAND} ${MEMORYCHECK_COMMAND_OPTIONS} $<TARGET_FILE:null-args-doc>)
add_test(NAME null-args-covered
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/null/covered.sh ${CMAKE_SOURCE_DIR})

# Every name tld.h gives a program follows the naming rules (tld_/TLD_, internals
# tld__/TLD__), and so does what tld_doc.h adds (tld_doc_/TLD_DOC_, internals
# tld__doc_/TLD__DOC_); a program may reuse common names after the implementation.
find_program(NM_PROGRAM nm)
add_test(NAME names
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/names/check.sh ${CMAKE_C_COMPILER} ${CMAKE_SOURCE_DIR} ${NM_PROGRAM})

# The compiler rejects the mistakes the headers are designed to catch (text as a
# printf format, free() on library memory, writable pointers into a hash, gfree(&p)).
add_test(NAME misuse-rejected
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/misuse/check.sh ${CMAKE_C_COMPILER} ${CMAKE_SOURCE_DIR})

# tld_ai.h: mock-server tests, compile checks, sanitizer runs
include(${CMAKE_CURRENT_SOURCE_DIR}/ai.cmake)

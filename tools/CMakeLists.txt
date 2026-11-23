# tlddoc writes docs/tld.md and docs/tld_ai.md from the comments in tld.h and tld_ai.h.
#   cmake --build build --target docs     regenerate the manuals
#   ctest -R docs-current                 fails while they are out of date
#   ctest -R doc-examples                 fails while a doc-comment example does not compile or run
find_package(Threads REQUIRED)

add_executable(tlddoc tlddoc.c)
set_target_properties(tlddoc PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
target_link_libraries(tlddoc PRIVATE m Threads::Threads)

add_custom_target(docs
  COMMAND tlddoc ${CMAKE_SOURCE_DIR}
  DEPENDS tlddoc
  COMMENT "Regenerating docs/tld.md and docs/tld_ai.md"
  VERBATIM)

add_test(NAME docs-current COMMAND tlddoc --check ${CMAKE_SOURCE_DIR})

# Every code example in the doc comments compiles and runs against the current headers;
# the tld_ai.h ones against tests/ai/mock_ollama.py, a mock server (never a real Ollama).
# The build's C flags go along, so a sanitizer build runs the examples with sanitizers.
string(TOUPPER "${CMAKE_BUILD_TYPE}" TLDDOC_BT)
set(TLDDOC_EX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${TLDDOC_BT}}")
if(CMAKE_BUILD_TYPE MATCHES ASAN)       # its -fsanitize=address is a compile option
  string(APPEND TLDDOC_EX_FLAGS " -fsanitize=address")
endif()
add_test(NAME doc-examples
  COMMAND sh ${CMAKE_SOURCE_DIR}/tests/docs/examples.sh $<TARGET_FILE:tlddoc>
          ${CMAKE_C_COMPILER} ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/doc-examples tld "${TLDDOC_EX_FLAGS}")
add_test(NAME doc-examples-ai-mock
  COMMAND sh ${CMAKE_SOURCE_DIR}/tests/docs/examples.sh $<TARGET_FILE:tlddoc>
          ${CMAKE_C_COMPILER} ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/doc-examples-ai ai "${TLDDOC_EX_FLAGS}")

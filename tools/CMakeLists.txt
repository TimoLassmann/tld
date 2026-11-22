# tlddoc writes docs/tld.md and docs/tld_ai.md from the comments in tld.h and tld_ai.h.
#   cmake --build build --target docs     regenerate the manuals
#   ctest -R docs-current                 fails while they are out of date
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

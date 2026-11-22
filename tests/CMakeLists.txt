cmake_minimum_required(VERSION 3.18)

project(tld-tests LANGUAGES C)

find_program(MEMORYCHECK_COMMAND valgrind NO_CACHE)
if (MEMORYCHECK_COMMAND)
  message(STATUS "Found Valgrind: ${MEMORYCHECK_COMMAND}")
  set(MEMORYCHECK_COMMAND ${MEMORYCHECK_COMMAND})
  #set(MEMORYCHECK_COMMAND_OPTIONS "--show-leak-kinds=all")
  set(MEMORYCHECK_COMMAND_OPTIONS
    "--leak-check=full"
    "--show-leak-kinds=all"
    "--error-exitcode=1"
  )
  #set(MEMORYCHECK_COMMAND_OPTIONS "--leak-check=full --show-leak-kinds=all --exit-on-first-error=yes --error-exitcode=1")
else()
  message(STATUS "Valgrind not found")
  set(MEMORYCHECK_COMMAND "")
  set(MEMORYCHECK_COMMAND_OPTIONS "")
  
endif()



function(add_custom_test TEST_NAME)
  add_executable(${TEST_NAME} ${ARGN})
  target_link_libraries(${TEST_NAME}
    PRIVATE
    tld-dev
  )
  # message(STATUS "Running test: ${TEST_NAME} with command: ${MEMORYCHECK_COMMAND} ${MEMORYCHECK_COMMAND_OPTIONS} ${TEST_NAME}")
  add_test(
    NAME ${TEST_NAME}
    COMMAND ${MEMORYCHECK_COMMAND} ${MEMORYCHECK_COMMAND_OPTIONS} ${CMAKE_BINARY_DIR}/tests/${TEST_NAME}
  )
endfunction()

# add_executable(unit-generic utests/generic.c) 
# target_link_libraries(unit-generic PRIVATE tld-dev)
find_package(Threads REQUIRED)
if(Threads_FOUND)
  add_executable(unit-tpool
    utests/unit_tpool.c
  )
  target_link_libraries(unit-tpool PRIVATE tld-dev Threads::Threads)
endif(Threads_FOUND)

add_custom_test(unit-template utests/unit_template.c)
add_custom_test(unit-hash utests/unit_hash.c)

add_custom_test(unit-core utests/unit_core.c)
add_custom_test(unit-bit utests/unit_bitfield.c)
add_custom_test(unit-kdtree utests/unit_kdtree.c)
add_custom_test(unit-json utests/unit_json.c)

add_custom_test(unit-qnorm utests/unit_qnorm.c)
add_custom_test(unit-auc utests/unit_auc.c)
add_custom_test(unit-shuffle utests/unit_shuffle.c)
add_custom_test(unit-misc utests/unit_misc.c)
add_custom_test(unit-timing utests/unit_timing.c)
add_custom_test(unit-sort utests/unit_sort.c)
add_custom_test(unit-sigmoid utests/unit_sigmoid.c)
add_custom_test(kde_itest itests/kde_itest.c) 
add_custom_test(unit-str utests/unit_str.c) 
add_custom_test(int-noise itests/noise.c) 
add_custom_test(unit-stats utests/unit_stats.c) 
add_custom_test(unit-pearson utests/unit_pearson.c) 
add_custom_test(unit-test-suite utests/unit_tests.c) 
add_custom_test(unit-rng utests/unit_rng.c) 
add_custom_test(unit-logsum utests/unit_logsum.c) 
add_custom_test(unit-align utests/unit_align.c)

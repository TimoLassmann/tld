find_package(Threads REQUIRED)
if(Threads_FOUND)
  message(STATUS "Threads are enabled.")
  add_definitions (-DHAVE_PTHREAD)
endif(Threads_FOUND)

add_library(tld-dev STATIC
  tld.c
  )

target_include_directories(tld-dev
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
  )

target_link_libraries(tld-dev PRIVATE m Threads::Threads)

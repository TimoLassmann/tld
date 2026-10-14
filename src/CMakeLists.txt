# Option to enable/disable HDF5 support
option(TLD_USE_HDF5 "Enable HDF5 support" OFF)

if(TLD_USE_HDF5)
  find_package(HDF5 REQUIRED COMPONENTS C HL)
  set(HDF5_USE_STATIC_LIBRARIES ON)
endif()

find_package(ZLIB)

find_package(Threads REQUIRED)
if(Threads_FOUND)
  message(STATUS "Threads are enabled.")
  add_definitions (-DHAVE_PTHREAD)
endif(Threads_FOUND)


if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/tld-seq-tables.h)
  #add some compilation flags

  add_library(tld-dev STATIC
    tld.c
    ${CMAKE_CURRENT_BINARY_DIR}/tld-seq-tables.h
    )



  target_include_directories(tld-dev
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    )

  if(TLD_USE_HDF5)
    target_link_libraries(tld-dev PRIVATE m ZLIB::ZLIB Threads::Threads ${HDF5_C_LIBRARIES})
    target_include_directories(tld-dev PUBLIC ${HDF5_C_INCLUDE_DIRS})
  else()
    target_link_libraries(tld-dev PRIVATE m ZLIB::ZLIB Threads::Threads)
    target_compile_definitions(tld-dev PUBLIC TLD_NO_HDF5)
  endif()
else()
  add_library(boot STATIC
    tld.c
    )
  target_compile_options(boot PRIVATE -DBOOT)
  target_link_libraries(boot  PRIVATE m ZLIB::ZLIB Threads::Threads)

  add_executable(gentable
    gentables.c
    )
  target_compile_options(gentable PRIVATE -DBOOT)
  target_link_libraries(gentable PRIVATE boot)

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tld-seq-tables.h
    COMMAND gentable ${CMAKE_CURRENT_BINARY_DIR}/tld-seq-tables.h
    DEPENDS gentable
    )

  add_library(tld-dev STATIC
    tld.c
    ${CMAKE_CURRENT_BINARY_DIR}/tld-seq-tables.h
    )

  target_include_directories(tld-dev
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    )

  if(TLD_USE_HDF5)
    target_link_libraries(tld-dev PRIVATE m ZLIB::ZLIB Threads::Threads ${HDF5_C_LIBRARIES})
    target_include_directories(tld-dev PUBLIC ${HDF5_C_INCLUDE_DIRS})
  else()
    target_link_libraries(tld-dev PRIVATE m ZLIB::ZLIB Threads::Threads)
    target_compile_definitions(tld-dev PUBLIC TLD_NO_HDF5)
  endif()

endif(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/tld-seq-tables.h)

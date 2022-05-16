
find_package(HDF5 REQUIRED COMPONENTS C HL)
find_package(ZLIB)

add_library(tld-dev STATIC
  tld.c
  )


target_include_directories(tld-dev
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
  )
target_link_libraries(tld-dev PRIVATE m ZLIB::ZLIB HDF5::HDF5 ${HDF5_LIBRARIES})
target_include_directories(tld-dev INTERFACE ${HDF5_C_INCLUDE_DIRS})

target_compile_definitions(tld-dev INTERFACE ${HDF5_DEFINITIONS})


# if(HDF5_FOUND)
#   message(STATUS "HDF5 was  found")
#   set_target_properties(tld-dev PROPERTIES COMPILE_FLAGS "-DHAVE_HDF5")
  
# endif()




# include_directories(${CMAKE_CURRENT_BINARY_DIR})

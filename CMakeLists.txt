cmake_minimum_required(VERSION 3.1)

project(tsl_ordered_map)

add_library(tsl_ordered_map INTERFACE)
# Use tsl::ordered_map as target, more consistent with other libraries conventions (Boost, Qt, ...)
add_library(tsl::ordered_map ALIAS tsl_ordered_map)

target_include_directories(tsl_ordered_map INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_sources(tsl_ordered_map INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_hash.h"
                                         "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_map.h"
                                         "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_set.h")


if(${CMAKE_VERSION} VERSION_GREATER "3.7") 
    # Only available since version 3.8
    target_compile_features(tsl_ordered_map INTERFACE cxx_std_11)
endif()

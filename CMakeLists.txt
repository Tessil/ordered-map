cmake_minimum_required(VERSION 3.10)
include(GNUInstallDirs)


project(tsl-ordered-map VERSION 1.2.0)

add_library(ordered_map INTERFACE)
# Use tsl::ordered_map as target, more consistent with other libraries conventions (Boost, Qt, ...)
add_library(tsl::ordered_map ALIAS ordered_map)

target_include_directories(ordered_map INTERFACE
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

list(APPEND headers "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_hash.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_map.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_set.h")
target_sources(ordered_map INTERFACE "$<BUILD_INTERFACE:${headers}>")

if(MSVC)
    target_sources(ordered_map INTERFACE
                   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tsl-ordered-map.natvis>"
                   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_DATAROOTDIR}/tsl-ordered-map.natvis>")
endif()




option(TSL_ORDERED_MAP_BUILD_MODULE "Build the C++20 module for tsl::ordered_map" OFF)

if(TSL_ORDERED_MAP_BUILD_MODULE)
    if(CMAKE_CXX_STANDARD LESS 20)
        message(FATAL_ERROR "TSL_ORDERED_MAP_BUILD_MODULE requires C++20 or later (CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD})")
    endif()

    add_library(ordered_map_module)
    add_library(tsl::ordered_map_module ALIAS ordered_map_module)

    target_sources(ordered_map_module
                   PUBLIC FILE_SET CXX_MODULES FILES
                   "${CMAKE_CURRENT_SOURCE_DIR}/src/tsl.ordered_map.cppm")

    target_link_libraries(ordered_map_module PUBLIC tsl::ordered_map)
    set_target_properties(ordered_map_module PROPERTIES CXX_STANDARD 20)

    install(TARGETS ordered_map_module
            EXPORT tsl-ordered-mapTargets
            FILE_SET CXX_MODULES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endif()

include(CMakePackageConfigHelpers)

## Install include directory and potential natvis file
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(MSVC)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tsl-ordered-map.natvis"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}")
endif()



## Create and install tsl-ordered-mapConfig.cmake
configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/tsl-ordered-mapConfig.cmake.in"
                                "${CMAKE_CURRENT_BINARY_DIR}/tsl-ordered-mapConfig.cmake"
                                INSTALL_DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-ordered-map")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tsl-ordered-mapConfig.cmake"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-ordered-map")



## Create and install tsl-ordered-mapTargets.cmake
install(TARGETS ordered_map
        EXPORT tsl-ordered-mapTargets)

install(EXPORT tsl-ordered-mapTargets
        NAMESPACE tsl::
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-ordered-map")



## Create and install tsl-ordered-mapConfigVersion.cmake
# tsl-ordered-map is header-only and does not depend on the architecture.
# Remove CMAKE_SIZEOF_VOID_P from tsl-ordered-mapConfigVersion.cmake so that a
# tsl-ordered-mapConfig.cmake generated for a 64 bit target can be used for 32 bit
# targets and vice versa.
set(CMAKE_SIZEOF_VOID_P_BACKUP ${CMAKE_SIZEOF_VOID_P})
unset(CMAKE_SIZEOF_VOID_P)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/tsl-ordered-mapConfigVersion.cmake"
                                COMPATIBILITY SameMajorVersion)
set(CMAKE_SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P_BACKUP})

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tsl-ordered-mapConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/tsl-ordered-map")

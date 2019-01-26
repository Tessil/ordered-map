cmake_minimum_required(VERSION 3.1)

project(tsl_ordered_map VERSION 0.7.1)

add_library(ordered_map INTERFACE)
# Use tsl::ordered_map as target, more consistent with other libraries conventions (Boost, Qt, ...)
add_library(tsl::ordered_map ALIAS ordered_map)

target_include_directories(ordered_map INTERFACE
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                           "$<INSTALL_INTERFACE:include>")

list(APPEND headers "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_hash.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_map.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_set.h")
target_sources(ordered_map INTERFACE "$<BUILD_INTERFACE:${headers}>")

if(MSVC)
    target_sources(ordered_map INTERFACE
                   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tsl-ordered-map.natvis>"
                   "$<INSTALL_INTERFACE:tsl-ordered-map.natvis>")
endif()

# Only compatible with CMake version >= 3.8
if(${CMAKE_VERSION} VERSION_GREATER "3.7") 
    target_compile_features(ordered_map INTERFACE cxx_std_11)
endif()




# Installation (only compatible with CMake version >= 3.3)
if(${CMAKE_VERSION} VERSION_GREATER "3.2")
    include(CMakePackageConfigHelpers)
    include(GNUInstallDirs)

    ## Install include directory and potential natvis file
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/tsl"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    if(MSVC)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tsl-ordered-map.natvis"
                DESTINATION ".")
    endif()



    ## Create and install tsl_ordered_mapConfig.cmake
    configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/tsl_ordered_mapConfig.cmake.in"
                                  "${CMAKE_CURRENT_BINARY_DIR}/tsl_ordered_mapConfig.cmake"
                                  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tsl_ordered_map")

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tsl_ordered_mapConfig.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tsl_ordered_map")

            

    ## Create and install tsl_ordered_mapTargets.cmake
    install(TARGETS ordered_map
            EXPORT tsl_ordered_mapTargets)
            
    install(EXPORT tsl_ordered_mapTargets
            NAMESPACE tsl::
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tsl_ordered_map")

            

    ## Create and install tsl_ordered_mapConfigVersion.cmake
    # tsl_ordered_map is header-only and does not depend on the architecture.
    # Remove CMAKE_SIZEOF_VOID_P from tsl_ordered_mapConfigVersion.cmake so that an
    # tsl_ordered_mapConfig.cmake generated for a 64 bit target can be used for 32 bit
    # targets and vice versa.
    set(TSL_ORDERED_MAP_CMAKE_SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P})
    unset(CMAKE_SIZEOF_VOID_P)
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/tsl_ordered_mapConfigVersion.cmake"
                                     COMPATIBILITY SameMajorVersion)
    set(CMAKE_SIZEOF_VOID_P ${TSL_ORDERED_MAP_CMAKE_SIZEOF_VOID_P})

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tsl_ordered_mapConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tsl_ordered_map")
endif()

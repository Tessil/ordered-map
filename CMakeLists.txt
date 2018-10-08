cmake_minimum_required(VERSION 3.1)

project(tsl_ordered_map VERSION 0.7.0)

add_library(tsl_ordered_map INTERFACE)
# Use tsl::ordered_map as target, more consistent with other libraries conventions (Boost, Qt, ...)
add_library(tsl::ordered_map ALIAS tsl_ordered_map)

target_include_directories(tsl_ordered_map INTERFACE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:include>)
target_sources(tsl_ordered_map INTERFACE 
               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_hash.h
                                 ${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_map.h
                                 ${CMAKE_CURRENT_SOURCE_DIR}/include/tsl/ordered_set.h>)

if(MSVC)
    target_sources(tsl_ordered_map INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/tsl_ordered_map.natvis")
endif()

if(${CMAKE_VERSION} VERSION_GREATER "3.7") 
    # Only available since version 3.8
    target_compile_features(tsl_ordered_map INTERFACE cxx_std_11)
endif()

# Installation
# ============

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS tsl_ordered_map
        EXPORT ${PROJECT_NAME}-targets)

# Makes the project importable from the build directory
export(EXPORT ${PROJECT_NAME}-targets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/tsl
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(ORDERED_MAP_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE
    STRING "install path for xtlConfig.cmake")

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${ORDERED_MAP_CMAKECONFIG_INSTALL_DIR})

# ordered_map is header-only and does not depend on the architecture.
# Remove CMAKE_SIZEOF_VOID_P from tsl_ordered_mapConfigVersion.cmake so that an
# tsl_ordered_mapConfig.cmake generated for a 64 bit target can be used for 32 bit
# targets and vice versa.
set(_ORDERED_MAP_CMAKE_SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P})
unset(CMAKE_SIZEOF_VOID_P)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                 COMPATIBILITY SameMajorVersion)
set(CMAKE_SIZEOF_VOID_P ${_ORDERED_MAP_CMAKE_SIZEOF_VOID_P})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
DESTINATION ${ORDERED_MAP_CMAKECONFIG_INSTALL_DIR})
install(EXPORT ${PROJECT_NAME}-targets
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION ${ORDERED_MAP_CMAKECONFIG_INSTALL_DIR})


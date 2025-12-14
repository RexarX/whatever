# Client Third-party Dependencies
# This file adds the root third_party directory which contains doctest

# Add root third_party directory
set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Project root directory")

if(EXISTS "${PROJECT_ROOT_DIR}/third_party/CMakeLists.txt")
    add_subdirectory("${PROJECT_ROOT_DIR}/third_party" "${CMAKE_BINARY_DIR}/root_third_party")
    message(STATUS "Added root third_party directory for doctest")
else()
    message(WARNING "Root third_party directory not found at: ${PROJECT_ROOT_DIR}/third_party")
endif()

# ctti is local to client
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ctti/include")
    add_subdirectory(ctti)
    add_library(client::ctti ALIAS ctti)
    message(STATUS "  ✓ ctti configured (submodule)")
else()
    message(WARNING "ctti submodule not found at third_party/ctti")
    message(WARNING "Run: git submodule update --init --recursive")
endif()

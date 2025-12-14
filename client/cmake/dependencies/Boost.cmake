# This module handles finding Boost from multiple sources:
# 1. Conan (if using Conan)
# 2. System packages (pacman, apt, etc.)
# 3. CPM download (fallback)
#
# Boost is only needed if std::stacktrace is not available (C++23 feature)

include_guard(GLOBAL)

cmake_policy(SET CMP0167 OLD)
message(STATUS "Configuring Boost dependency...")

# Check if std::stacktrace is available
# This requires C++23 and a sufficiently new compiler
# Note: GCC requires linking with -lstdc++exp or -lstdc++_libbacktrace for std::stacktrace
include(CheckCXXSourceCompiles)

# First check if the header and feature test macro exist
set(CMAKE_REQUIRED_FLAGS "-std=c++23")

# For GCC, we need to link with stdc++exp or stdc++_libbacktrace
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Try with stdc++exp first (newer GCC)
    set(CMAKE_REQUIRED_LIBRARIES "stdc++exp")
    check_cxx_source_compiles("
#include <version>
#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
#include <stacktrace>
int main() {
    auto st = std::stacktrace::current();
    return st.size() >= 0 ? 0 : 1;
}
#else
#error std::stacktrace not available
#endif
" CLIENT_HAS_STD_STACKTRACE_WITH_STDCXXEXP)

    if(CLIENT_HAS_STD_STACKTRACE_WITH_STDCXXEXP)
        set(CLIENT_HAS_STD_STACKTRACE TRUE)
        set(CLIENT_STD_STACKTRACE_LIBRARY "stdc++exp")
    else()
        # Try with stdc++_libbacktrace
        set(CMAKE_REQUIRED_LIBRARIES "stdc++_libbacktrace")
        check_cxx_source_compiles("
#include <version>
#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
#include <stacktrace>
int main() {
    auto st = std::stacktrace::current();
    return st.size() >= 0 ? 0 : 1;
}
#else
#error std::stacktrace not available
#endif
" CLIENT_HAS_STD_STACKTRACE_WITH_LIBBACKTRACE)
        if(CLIENT_HAS_STD_STACKTRACE_WITH_LIBBACKTRACE)
            set(CLIENT_HAS_STD_STACKTRACE TRUE)
            set(CLIENT_STD_STACKTRACE_LIBRARY "stdc++_libbacktrace")
        else()
            set(CLIENT_HAS_STD_STACKTRACE FALSE)
        endif()
    endif()
    unset(CMAKE_REQUIRED_LIBRARIES)
else()
    # For other compilers (Clang, MSVC), just check normally
    check_cxx_source_compiles("
#include <version>
#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
#include <stacktrace>
int main() {
    auto st = std::stacktrace::current();
    return st.size() >= 0 ? 0 : 1;
}
#else
#error std::stacktrace not available
#endif
" CLIENT_HAS_STD_STACKTRACE)
    set(CLIENT_STD_STACKTRACE_LIBRARY "")
endif()

unset(CMAKE_REQUIRED_FLAGS)

if(CLIENT_HAS_STD_STACKTRACE)
    message(STATUS "  ✓ std::stacktrace is available, Boost stacktrace not needed")
    if(CLIENT_STD_STACKTRACE_LIBRARY)
        message(STATUS "    Using library: ${CLIENT_STD_STACKTRACE_LIBRARY}")
    endif()
    set(CLIENT_USE_BOOST_STACKTRACE OFF CACHE BOOL "Use Boost stacktrace instead of std::stacktrace" FORCE)

    # Create interface targets for compatibility and to link the required library
    if(NOT TARGET client::boost)
        add_library(client::boost INTERFACE IMPORTED GLOBAL)
    endif()
    if(NOT TARGET client::boost_stacktrace)
        add_library(client::boost_stacktrace INTERFACE IMPORTED GLOBAL)
        if(CLIENT_STD_STACKTRACE_LIBRARY)
            target_link_libraries(client::boost_stacktrace INTERFACE ${CLIENT_STD_STACKTRACE_LIBRARY})
        endif()
    endif()

    return()
endif()

message(STATUS "  std::stacktrace not available, using Boost stacktrace")
set(CLIENT_USE_BOOST_STACKTRACE ON CACHE BOOL "Use Boost stacktrace instead of std::stacktrace" FORCE)

# Boost components required by Client
set(CLIENT_BOOST_REQUIRED_COMPONENTS
    stacktrace
)

# Try to find Boost in order of preference
set(_boost_found_via "")

# 1. Try Conan first if using Conan and not forcing system packages
if(CLIENT_USE_CONAN AND NOT CLIENT_FORCE_SYSTEM_PACKAGES)
    find_package(Boost 1.82 QUIET CONFIG)
    if(Boost_FOUND)
        set(_boost_found_via "Conan")
    endif()
endif()

# 2. Try system package manager if not found via Conan
if(NOT Boost_FOUND)
    # Try CONFIG mode first (modern CMake packages like Arch Linux, Ubuntu 22.04+)
    find_package(Boost 1.82 QUIET CONFIG)
    if(Boost_FOUND)
        set(_boost_found_via "system (CONFIG)")
    else()
        # Fall back to MODULE mode with specific components
        find_package(Boost 1.82 QUIET COMPONENTS ${CLIENT_BOOST_REQUIRED_COMPONENTS})
        if(Boost_FOUND)
            set(_boost_found_via "system (MODULE)")
        endif()
    endif()
endif()

# 3. Create Boost targets if found
if(Boost_FOUND)
    if(_boost_found_via)
        message(STATUS "  ✓ Boost found via ${_boost_found_via}")
    else()
        message(STATUS "  ✓ Boost found at ${Boost_DIR}")
    endif()

    # Create convenience target: client::boost
    if(NOT TARGET client::boost)
        add_library(client::boost INTERFACE IMPORTED)
        target_link_libraries(client::boost INTERFACE Boost::boost)
    endif()

    # Create stacktrace-specific alias: client::boost_stacktrace
    if(NOT TARGET client::boost_stacktrace)
        add_library(client::boost_stacktrace INTERFACE IMPORTED GLOBAL)
        if(TARGET Boost::stacktrace)
            target_link_libraries(client::boost_stacktrace INTERFACE Boost::stacktrace)
        elseif(TARGET Boost::boost)
            # For header-only Boost (e.g., from Conan), link to main target
            target_link_libraries(client::boost_stacktrace INTERFACE Boost::boost)
        elseif(TARGET Boost::headers)
            # Alternative header-only target
            target_link_libraries(client::boost_stacktrace INTERFACE Boost::headers)
        endif()
    endif()

    # Create aliases for different stacktrace backends
    if(TARGET Boost::stacktrace_basic AND NOT TARGET client::boost_stacktrace_basic)
        add_library(client::boost_stacktrace_basic INTERFACE IMPORTED GLOBAL)
        target_link_libraries(client::boost_stacktrace_basic INTERFACE Boost::stacktrace_basic)
    endif()

    if(TARGET Boost::stacktrace_backtrace AND NOT TARGET client::boost_stacktrace_backtrace)
        add_library(client::boost_stacktrace_backtrace INTERFACE IMPORTED GLOBAL)
        target_link_libraries(client::boost_stacktrace_backtrace INTERFACE Boost::stacktrace_backtrace)
    endif()

    if(TARGET Boost::stacktrace_addr2line AND NOT TARGET client::boost_stacktrace_addr2line)
        add_library(client::boost_stacktrace_addr2line INTERFACE IMPORTED GLOBAL)
        target_link_libraries(client::boost_stacktrace_addr2line INTERFACE Boost::stacktrace_addr2line)
    endif()

    if(TARGET Boost::stacktrace_windbg AND NOT TARGET client::boost_stacktrace_windbg)
        add_library(client::boost_stacktrace_windbg INTERFACE IMPORTED GLOBAL)
        target_link_libraries(client::boost_stacktrace_windbg INTERFACE Boost::stacktrace_windbg)
    endif()


else()
    # 4. Try CPM fallback if system packages not found
    if(CLIENT_DOWNLOAD_PACKAGES)
        message(STATUS "  ⬇ Boost not found in system, downloading via CPM...")

        CPMAddPackage(
            NAME Boost
            VERSION 1.90.0
            URL https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0-cmake.tar.xz
            OPTIONS
                "BOOST_ENABLE_CMAKE ON"
                "BOOST_INCLUDE_LIBRARIES stacktrace"
                "BUILD_SHARED_LIBS OFF"
            SYSTEM YES
        )

        # Create aliases if Boost was just added or already cached
        if(Boost_ADDED OR TARGET Boost::boost)
            if(NOT TARGET client::boost)
                add_library(client::boost INTERFACE IMPORTED GLOBAL)
                if(TARGET Boost::boost)
                    target_link_libraries(client::boost INTERFACE Boost::boost)
                endif()
            endif()

            if(NOT TARGET client::boost_stacktrace)
                add_library(client::boost_stacktrace INTERFACE IMPORTED GLOBAL)
            endif()

            if(TARGET Boost::stacktrace)
                target_link_libraries(client::boost_stacktrace INTERFACE Boost::stacktrace)
            else()
                target_link_libraries(client::boost_stacktrace INTERFACE client::boost)
            endif()

            if(TARGET Boost::stacktrace_backtrace AND NOT TARGET client::boost_stacktrace_backtrace)
                add_library(client::boost_stacktrace_backtrace INTERFACE IMPORTED GLOBAL)
                target_link_libraries(client::boost_stacktrace_backtrace INTERFACE Boost::stacktrace_backtrace)
            endif()

            if(TARGET Boost::stacktrace_addr2line AND NOT TARGET client::boost_stacktrace_addr2line)
                add_library(client::boost_stacktrace_addr2line INTERFACE IMPORTED GLOBAL)
                target_link_libraries(client::boost_stacktrace_addr2line INTERFACE Boost::stacktrace_addr2line)
            endif()

            message(STATUS "  ✓ Boost downloaded and configured via CPM")
        endif()

        set(Boost_FOUND TRUE PARENT_SCOPE)
    else()
        message(WARNING "  ✗ Boost not found and CLIENT_DOWNLOAD_PACKAGES is OFF. Stacktrace support will be limited.")
        set(Boost_FOUND FALSE PARENT_SCOPE)

        # Create empty interface targets to avoid link errors
        if(NOT TARGET client::boost)
            add_library(client::boost INTERFACE IMPORTED GLOBAL)
        endif()
        if(NOT TARGET client::boost_stacktrace)
            add_library(client::boost_stacktrace INTERFACE IMPORTED GLOBAL)
        endif()
    endif()
endif()

# Dynamically create aliases for each Boost component
if(Boost_FOUND AND CLIENT_USE_BOOST_STACKTRACE)
    foreach(component IN LISTS CLIENT_BOOST_REQUIRED_COMPONENTS)
        set(target_name "client::boost_${component}")

        if(TARGET "Boost::${component}")
            if(NOT TARGET ${target_name})
                add_library(${target_name} INTERFACE IMPORTED GLOBAL)
                target_link_libraries(${target_name} INTERFACE "Boost::${component}")
                message(STATUS "  ✓ Created alias: ${target_name}")
            endif()
            # Link the component to the main client::boost target
            target_link_libraries(client::boost INTERFACE "Boost::${component}")
        else()
            message(STATUS "  ⚠ Boost component '${component}' not found as separate target")
        endif()
    endforeach()
endif()

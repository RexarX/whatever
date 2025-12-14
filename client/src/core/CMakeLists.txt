# Client Core Library
# Static library containing logging, assertions, and utility functions.

cmake_minimum_required(VERSION 3.25)

# Core library sources
set(CLIENT_CORE_SOURCES
    src/assert.cpp
    src/logger.cpp
    src/pch.cpp
)

# Core library headers
set(CLIENT_CORE_HEADERS
    include/client/core/assert.hpp
    include/client/core/core.hpp
    include/client/core/logger.hpp
    include/client/core/pch.hpp

    include/client/core/utils/fast_pimpl.hpp
    include/client/core/utils/filesystem.hpp
)

# Create static library
add_library(client_core STATIC
    ${CLIENT_CORE_SOURCES}
    ${CLIENT_CORE_HEADERS}
)
add_library(client::core ALIAS client_core)

# Enable PIC for linking into shared libraries
set_target_properties(client_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

client_target_set_warnings(client_core)
client_target_set_optimization(client_core)
client_target_set_output_dirs(client_core)
client_target_add_pch(client_core ${CMAKE_CURRENT_SOURCE_DIR}/include/client/core/pch.hpp PUBLIC)
client_target_set_folder(client_core "Libraries")

# Platform-specific configurations for boost stacktrace
if(CLIENT_USE_BOOST_STACKTRACE)
    if(UNIX AND NOT APPLE AND NOT ANDROID)
        find_library(BACKTRACE_LIBRARY backtrace)
        if(BACKTRACE_LIBRARY)
            target_link_libraries(client_core PRIVATE ${BACKTRACE_LIBRARY})
            target_compile_definitions(client_core PRIVATE BOOST_STACKTRACE_USE_BACKTRACE)
        endif()
    elseif(WIN32)
        target_compile_definitions(client_core PRIVATE BOOST_STACKTRACE_USE_WINDBG)
    endif()
endif()

# Static library - no need for export macros
target_compile_definitions(client_core
    PUBLIC
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:
            CLIENT_ENABLE_ASSERTS
            CLIENT_ENABLE_STACKTRACE
        >
        $<$<AND:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,$<BOOL:${CLIENT_HAS_STD_STACKTRACE}>>:
            CLIENT_USE_STD_STACKTRACE
        >
)

# Include directories
target_include_directories(client_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(client_core PUBLIC
    ctti::ctti
    client::qt6::Core
)

# Link boost stacktrace if available and std::stacktrace is not
if(TARGET client::boost_stacktrace)
    target_link_libraries(client_core PRIVATE client::boost_stacktrace)
endif()

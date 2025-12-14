# Provides helper functions for configuring ESP-IDF build targets

include(CheckIPOSupported)

# ============================================================================
# Warning Configuration
# ============================================================================

# Apply standard compiler warnings to a target
function(embedded_target_set_warnings TARGET)
    set(GCC_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
    )

    if(EMBEDDED_ENABLE_WARNINGS_AS_ERRORS)
        list(APPEND GCC_WARNINGS -Werror)
    endif()

    target_compile_options(${TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:${GCC_WARNINGS}>
    )
endfunction()

# ============================================================================
# Optimization Configuration
# ============================================================================

# Apply standard optimization flags to a target
function(embedded_target_set_optimization TARGET)
    target_compile_options(${TARGET} PRIVATE
        $<$<CONFIG:Debug>:-O0 -g3 -ggdb>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g -DNDEBUG>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endfunction()

# ============================================================================
# Platform Configuration
# ============================================================================

# Apply platform-specific compile definitions to a target
function(embedded_target_set_platform TARGET)
    target_compile_definitions(${TARGET} PUBLIC
        $<$<CONFIG:Debug>:EMBEDDED_DEBUG_MODE>
        $<$<CONFIG:RelWithDebInfo>:EMBEDDED_RELEASE_WITH_DEBUG_INFO_MODE>
        $<$<CONFIG:Release>:EMBEDDED_RELEASE_MODE NDEBUG>
    )
endfunction()

# ============================================================================
# Output Directory Configuration
# ============================================================================

# Set standard output directories for a target
function(embedded_target_set_output_dirs TARGET)
    cmake_parse_arguments(ARG "" "CUSTOM_FOLDER" "" ${ARGN})

    if(NOT ARG_CUSTOM_FOLDER)
        set(ARG_CUSTOM_FOLDER "")
    endif()

    if(ARG_CUSTOM_FOLDER)
        set(_output_dir "${EMBEDDED_ROOT_DIR}/bin/${ARG_CUSTOM_FOLDER}/$<LOWER_CASE:$<CONFIG>>-$<LOWER_CASE:${CMAKE_SYSTEM_NAME}>-$<LOWER_CASE:${CMAKE_SYSTEM_PROCESSOR}>")
    else()
        set(_output_dir "${EMBEDDED_ROOT_DIR}/bin/$<LOWER_CASE:$<CONFIG>>-$<LOWER_CASE:${CMAKE_SYSTEM_NAME}>-$<LOWER_CASE:${CMAKE_SYSTEM_PROCESSOR}>")
    endif()

    set_target_properties(${TARGET} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${_output_dir}"
        LIBRARY_OUTPUT_DIRECTORY "${_output_dir}"
        ARCHIVE_OUTPUT_DIRECTORY "${_output_dir}"
    )
endfunction()

# ============================================================================
# C++ Standard Configuration
# ============================================================================

# Set C++ standard for a target
function(embedded_target_set_cxx_standard TARGET)
    cmake_parse_arguments(ARG "" "STANDARD" "" ${ARGN})

    if(NOT ARG_STANDARD)
        set(ARG_STANDARD 23)
    endif()

    set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD ${ARG_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endfunction()

# ============================================================================
# IDE Organization
# ============================================================================

# Set IDE folder for a target
function(embedded_target_set_folder TARGET FOLDER_NAME)
    set_target_properties(${TARGET} PROPERTIES FOLDER ${FOLDER_NAME})
endfunction()

# ============================================================================
# Test Configuration
# ============================================================================

# Add a unit test target for embedded
# Usage: embedded_add_unit_test(
#     NAME test_name
#     SOURCES file1.cpp file2.cpp
#     [LIBRARIES lib1 lib2 ...]
#     [INCLUDE_DIRS dir1 dir2 ...]
#     [MODULE module_name]
# )
function(embedded_add_unit_test)
    cmake_parse_arguments(
        ARG
        ""
        "NAME;MODULE"
        "SOURCES;LIBRARIES;INCLUDE_DIRS"
        ${ARGN}
    )

    if(NOT ARG_NAME)
        message(FATAL_ERROR "embedded_add_unit_test: NAME is required")
    endif()

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "embedded_add_unit_test: SOURCES is required")
    endif()

    # Create executable
    add_executable(${ARG_NAME} ${ARG_SOURCES})

    # Set C++ standard
    embedded_target_set_cxx_standard(${ARG_NAME})

    # Set warnings and optimization
    embedded_target_set_warnings(${ARG_NAME})
    embedded_target_set_optimization(${ARG_NAME})
    embedded_target_set_platform(${ARG_NAME})

    # Link test framework (doctest)
    if(NOT TARGET embedded::doctest)
        message(FATAL_ERROR "embedded::doctest target not found. Make sure doctest is available.")
    endif()
    target_link_libraries(${ARG_NAME} PRIVATE embedded::doctest)

    # Link additional libraries
    if(ARG_LIBRARIES)
        target_link_libraries(${ARG_NAME} PRIVATE ${ARG_LIBRARIES})
    endif()

    # Add include directories
    if(ARG_INCLUDE_DIRS)
        target_include_directories(${ARG_NAME} PRIVATE ${ARG_INCLUDE_DIRS})
    endif()

    # Set output directory
    embedded_target_set_output_dirs(${ARG_NAME} CUSTOM_FOLDER "tests/unit")

    # Add to CTest
    add_test(NAME ${ARG_NAME} COMMAND ${ARG_NAME})

    # Set test labels
    set_tests_properties(${ARG_NAME} PROPERTIES LABELS "unit")
    if(ARG_MODULE)
        set_tests_properties(${ARG_NAME} PROPERTIES LABELS "unit;${ARG_MODULE}")
    endif()

    # Add to custom test targets
    add_dependencies(unit_tests ${ARG_NAME})
    add_dependencies(all_tests ${ARG_NAME})

    if(ARG_MODULE)
        if(NOT TARGET ${ARG_MODULE}_tests)
            add_custom_target(${ARG_MODULE}_tests)
        endif()
        add_dependencies(${ARG_MODULE}_tests ${ARG_NAME})
    endif()

    # Set IDE folder
    embedded_target_set_folder(${ARG_NAME} "Tests/Unit")

    message(STATUS "Added unit test: ${ARG_NAME}")
endfunction()

# Add an integration test target for embedded
# Usage: embedded_add_integration_test(
#     NAME test_name
#     SOURCES file1.cpp file2.cpp
#     [LIBRARIES lib1 lib2 ...]
#     [INCLUDE_DIRS dir1 dir2 ...]
#     [MODULE module_name]
# )
function(embedded_add_integration_test)
    cmake_parse_arguments(
        ARG
        ""
        "NAME;MODULE"
        "SOURCES;LIBRARIES;INCLUDE_DIRS"
        ${ARGN}
    )

    if(NOT ARG_NAME)
        message(FATAL_ERROR "embedded_add_integration_test: NAME is required")
    endif()

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "embedded_add_integration_test: SOURCES is required")
    endif()

    # Create executable
    add_executable(${ARG_NAME} ${ARG_SOURCES})

    # Set C++ standard
    embedded_target_set_cxx_standard(${ARG_NAME})

    # Set warnings and optimization
    embedded_target_set_warnings(${ARG_NAME})
    embedded_target_set_optimization(${ARG_NAME})
    embedded_target_set_platform(${ARG_NAME})

    # Link test framework (doctest)
    if(NOT TARGET embedded::doctest)
        message(FATAL_ERROR "embedded::doctest target not found. Make sure doctest is available.")
    endif()
    target_link_libraries(${ARG_NAME} PRIVATE embedded::doctest)

    # Link additional libraries
    if(ARG_LIBRARIES)
        target_link_libraries(${ARG_NAME} PRIVATE ${ARG_LIBRARIES})
    endif()

    # Add include directories
    if(ARG_INCLUDE_DIRS)
        target_include_directories(${ARG_NAME} PRIVATE ${ARG_INCLUDE_DIRS})
    endif()

    # Set output directory
    embedded_target_set_output_dirs(${ARG_NAME} CUSTOM_FOLDER "tests/integration")

    # Add to CTest
    add_test(NAME ${ARG_NAME} COMMAND ${ARG_NAME})

    # Set test labels
    set_tests_properties(${ARG_NAME} PROPERTIES LABELS "integration")
    if(ARG_MODULE)
        set_tests_properties(${ARG_NAME} PROPERTIES LABELS "integration;${ARG_MODULE}")
    endif()

    # Add to custom test targets
    add_dependencies(integration_tests ${ARG_NAME})
    add_dependencies(all_tests ${ARG_NAME})

    if(ARG_MODULE)
        if(NOT TARGET ${ARG_MODULE}_tests)
            add_custom_target(${ARG_MODULE}_tests)
        endif()
        add_dependencies(${ARG_MODULE}_tests ${ARG_NAME})
    endif()

    # Set IDE folder
    embedded_target_set_folder(${ARG_NAME} "Tests/Integration")

    message(STATUS "Added integration test: ${ARG_NAME}")
endfunction()

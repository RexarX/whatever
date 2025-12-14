# TestHelper.cmake
# Helper utilities for test configuration and execution

include_guard(GLOBAL)

# ============================================================================
# Test Discovery
# ============================================================================

# Automatically discover and add doctest tests using doctest_discover_tests
function(embedded_discover_tests TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "embedded_discover_tests: Target ${TARGET} does not exist")
    endif()

    # Use doctest's built-in test discovery if available
    if(COMMAND doctest_discover_tests)
        doctest_discover_tests(${TARGET})
    endif()
endfunction()

# ============================================================================
# Test Configuration
# ============================================================================

# Configure test timeout
function(embedded_set_test_timeout TARGET TIMEOUT_SECONDS)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "embedded_set_test_timeout: Target ${TARGET} does not exist")
    endif()

    set_tests_properties(${TARGET} PROPERTIES TIMEOUT ${TIMEOUT_SECONDS})
endfunction()

# Configure test working directory
function(embedded_set_test_working_directory TARGET WORKING_DIR)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "embedded_set_test_working_directory: Target ${TARGET} does not exist")
    endif()

    set_tests_properties(${TARGET} PROPERTIES WORKING_DIRECTORY ${WORKING_DIR})
endfunction()

# Configure test environment variables
function(embedded_set_test_environment TARGET)
    cmake_parse_arguments(ARG "" "" "VARS" ${ARGN})

    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "embedded_set_test_environment: Target ${TARGET} does not exist")
    endif()

    if(ARG_VARS)
        set_tests_properties(${TARGET} PROPERTIES ENVIRONMENT "${ARG_VARS}")
    endif()
endfunction()

# ============================================================================
# Test Filtering
# ============================================================================

# Add test filters (regex patterns)
function(embedded_add_test_filter TARGET FILTER_PATTERN)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "embedded_add_test_filter: Target ${TARGET} does not exist")
    endif()

    get_tests_properties(${TARGET} PROPERTIES LABELS _current_labels)
    if(_current_labels)
        set_tests_properties(${TARGET} PROPERTIES LABELS "${_current_labels};${FILTER_PATTERN}")
    else()
        set_tests_properties(${TARGET} PROPERTIES LABELS "${FILTER_PATTERN}")
    endif()
endfunction()

# ============================================================================
# Test Execution Helpers
# ============================================================================

# Create a test suite (collection of related tests)
function(embedded_create_test_suite SUITE_NAME)
    cmake_parse_arguments(ARG "" "" "TESTS" ${ARGN})

    if(NOT ARG_TESTS)
        message(FATAL_ERROR "embedded_create_test_suite: TESTS is required")
    endif()

    # Create custom target for the suite
    add_custom_target(${SUITE_NAME}_suite)

    # Add all tests to the suite
    foreach(_test ${ARG_TESTS})
        if(TARGET ${_test})
            add_dependencies(${SUITE_NAME}_suite ${_test})

            # Add suite label to each test
            get_tests_properties(${_test} PROPERTIES LABELS _current_labels)
            if(_current_labels)
                set_tests_properties(${_test} PROPERTIES LABELS "${_current_labels};suite_${SUITE_NAME}")
            else()
                set_tests_properties(${_test} PROPERTIES LABELS "suite_${SUITE_NAME}")
            endif()
        else()
            message(WARNING "Test ${_test} does not exist, skipping in suite ${SUITE_NAME}")
        endif()
    endforeach()

    message(STATUS "Created test suite: ${SUITE_NAME}")
endfunction()

# ============================================================================
# Test Data Helpers
# ============================================================================

# Copy test data files to test binary directory
function(embedded_copy_test_data TARGET)
    cmake_parse_arguments(ARG "" "DESTINATION" "FILES" ${ARGN})

    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "embedded_copy_test_data: Target ${TARGET} does not exist")
    endif()

    if(NOT ARG_FILES)
        message(FATAL_ERROR "embedded_copy_test_data: FILES is required")
    endif()

    if(NOT ARG_DESTINATION)
        set(ARG_DESTINATION "$<TARGET_FILE_DIR:${TARGET}>/test_data")
    endif()

    foreach(_file ${ARG_FILES})
        add_custom_command(
            TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${ARG_DESTINATION}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_file}" "${ARG_DESTINATION}"
            COMMENT "Copying test data: ${_file}"
        )
    endforeach()
endfunction()

# ============================================================================
# Coverage Configuration
# ============================================================================

# Enable code coverage for a test target
function(embedded_enable_test_coverage TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "embedded_enable_test_coverage: Target ${TARGET} does not exist")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET} PRIVATE
            --coverage
            -fprofile-arcs
            -ftest-coverage
        )
        target_link_options(${TARGET} PRIVATE --coverage)
    else()
        message(WARNING "Code coverage not supported for compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()

# ============================================================================
# Sanitizer Configuration
# ============================================================================

# Enable address sanitizer for a test target
function(embedded_enable_address_sanitizer TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "embedded_enable_address_sanitizer: Target ${TARGET} does not exist")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET} PRIVATE
            -fsanitize=address
            -fno-omit-frame-pointer
        )
        target_link_options(${TARGET} PRIVATE -fsanitize=address)
    else()
        message(WARNING "Address sanitizer not supported for compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()

# Enable undefined behavior sanitizer for a test target
function(embedded_enable_ubsan TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "embedded_enable_ubsan: Target ${TARGET} does not exist")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET} PRIVATE
            -fsanitize=undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${TARGET} PRIVATE -fsanitize=undefined)
    else()
        message(WARNING "UBSan not supported for compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()

# Enable thread sanitizer for a test target
function(embedded_enable_thread_sanitizer TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "embedded_enable_thread_sanitizer: Target ${TARGET} does not exist")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET} PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
        )
        target_link_options(${TARGET} PRIVATE -fsanitize=thread)
    else()
        message(WARNING "Thread sanitizer not supported for compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()

# ============================================================================
# Test Reporting
# ============================================================================

# Configure test to output JUnit XML report
function(embedded_enable_junit_output TARGET OUTPUT_FILE)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "embedded_enable_junit_output: Target ${TARGET} does not exist")
    endif()

    # Add doctest JUnit reporter flag
    set_tests_properties(${TARGET} PROPERTIES
        ENVIRONMENT "DOCTEST_OPTIONS=--reporters=junit --out=${OUTPUT_FILE}"
    )
endfunction()

# ============================================================================
# Utility Functions
# ============================================================================

# Print test configuration summary
function(embedded_print_test_info TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "embedded_print_test_info: Target ${TARGET} does not exist")
    endif()

    get_target_property(_sources ${TARGET} SOURCES)
    get_target_property(_link_libs ${TARGET} LINK_LIBRARIES)
    get_target_property(_compile_opts ${TARGET} COMPILE_OPTIONS)

    message(STATUS "========== Test Target: ${TARGET} ==========")
    message(STATUS "  Sources: ${_sources}")
    message(STATUS "  Libraries: ${_link_libs}")
    message(STATUS "  Compile Options: ${_compile_opts}")
    message(STATUS "============================================")
endfunction()

# Test utilities for Client Project

include(TargetUtils)

function(client_add_test)
    set(options "")
    set(oneValueArgs TARGET MODULE_NAME TYPE)
    set(multiValueArgs SOURCES DEPENDENCIES)
    cmake_parse_arguments(CLIENT_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT CLIENT_TEST_TARGET)
        message(FATAL_ERROR "client_add_test: TARGET is required")
    endif()

    if(NOT CLIENT_TEST_MODULE_NAME)
        message(FATAL_ERROR "client_add_test: MODULE_NAME is required")
    endif()

    if(NOT CLIENT_TEST_TYPE)
        message(FATAL_ERROR "client_add_test: TYPE is required (unit or integration)")
    endif()

    if(NOT CLIENT_TEST_SOURCES)
        message(FATAL_ERROR "client_add_test: SOURCES is required")
    endif()

    # Validate TEST_TYPE
    if(NOT CLIENT_TEST_TYPE STREQUAL "unit" AND NOT CLIENT_TEST_TYPE STREQUAL "integration")
        message(FATAL_ERROR "client_add_test: TYPE must be 'unit' or 'integration'")
    endif()

    # Create test executable
    add_executable(${CLIENT_TEST_TARGET} ${CLIENT_TEST_SOURCES})

    client_target_set_cxx_standard(${CLIENT_TEST_TARGET} STANDARD 23)
    client_target_set_optimization(${CLIENT_TEST_TARGET})
    client_target_set_warnings(${CLIENT_TEST_TARGET})
    client_target_set_output_dirs(${CLIENT_TEST_TARGET} CUSTOM_FOLDER tests)
    client_target_set_folder(${CLIENT_TEST_TARGET} "Client/Tests")

    # Enable LTO if requested
    if(CLIENT_ENABLE_LTO)
        client_target_enable_lto(${CLIENT_TEST_TARGET})
    endif()

    # Link doctest
    if(TARGET doctest::doctest)
        target_link_libraries(${CLIENT_TEST_TARGET} PRIVATE doctest::doctest)
    elseif(TARGET doctest)
        target_link_libraries(${CLIENT_TEST_TARGET} PRIVATE doctest)
    else()
        message(FATAL_ERROR "doctest target not found")
    endif()

    # Link additional dependencies if provided
    if(CLIENT_TEST_DEPENDENCIES)
        target_link_libraries(${CLIENT_TEST_TARGET} PRIVATE
            ${CLIENT_TEST_DEPENDENCIES}
        )
    endif()

    # Additional compile definitions to match the module PCH
    # Note: Do NOT add DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN if using doctest_with_main target
    # as it already has this defined
    target_compile_definitions(${CLIENT_TEST_TARGET} PRIVATE
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        $<$<PLATFORM_ID:Windows>:UNICODE>
        $<$<PLATFORM_ID:Windows>:_UNICODE>
    )

    # Create PCH for test target using the same header as the module
    # Use the absolute path to the module's PCH header
    if(TARGET client)
        # Construct the path to the module's PCH header
        set(_pch_path "${CLIENT_ROOT_DIR}/include/client/client_pch.hpp")
        if(EXISTS ${_pch_path})
            target_precompile_headers(${CLIENT_TEST_TARGET} PRIVATE ${_pch_path})
        endif()
    endif()

    # Add to CTest
    add_test(NAME ${CLIENT_TEST_TARGET} COMMAND ${CLIENT_TEST_TARGET})

    # Set test labels for filtering
    set_tests_properties(${CLIENT_TEST_TARGET} PROPERTIES
        LABELS "${CLIENT_TEST_MODULE_NAME};${CLIENT_TEST_TYPE}"
    )

    # Register with our custom targets
    register_test_target(${CLIENT_TEST_MODULE_NAME} ${CLIENT_TEST_TYPE} ${CLIENT_TEST_TARGET})
endfunction()

# Internal function to register test with build targets
function(register_test_target MODULE_NAME TEST_TYPE TARGET_NAME)
    # Add to global targets
    add_dependencies(all_tests ${TARGET_NAME})

    if(TEST_TYPE STREQUAL "unit")
        add_dependencies(unit_tests ${TARGET_NAME})
        add_dependencies(${MODULE_NAME}_unit_tests ${TARGET_NAME})
    elseif(TEST_TYPE STREQUAL "integration")
        add_dependencies(integration_tests ${TARGET_NAME})
        add_dependencies(${MODULE_NAME}_integration_tests ${TARGET_NAME})
    endif()

    # Add to module-specific target
    add_dependencies(${MODULE_NAME}_tests ${TARGET_NAME})
endfunction()

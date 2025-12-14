set(UNIT_TESTS_SOURCES
    # Core module tests
    unit/assert.cpp
    unit/core.cpp
    unit/logger.cpp

    # Utils tests
    unit/utils/filesystem.cpp
    unit/utils/fast_pimpl.cpp

    unit/main.cpp
)

client_add_test(
    TARGET client_core_unit
    MODULE_NAME core
    TYPE unit
    SOURCES ${UNIT_TESTS_SOURCES}
    DEPENDENCIES client_core
)

set(INTEGRATION_TESTS_SOURCES
    integration/logger.cpp

    integration/main.cpp
)

client_add_test(
    TARGET client_core_integration
    MODULE_NAME core
    TYPE integration
    SOURCES ${INTEGRATION_TESTS_SOURCES}
    DEPENDENCIES client_core
)

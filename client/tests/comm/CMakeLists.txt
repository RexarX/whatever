set(UNIT_TESTS_SOURCES
    unit/protocol.cpp
    unit/bluetooth.cpp

    unit/main.cpp
)

client_add_test(
    TARGET client_comm_unit
    MODULE_NAME comm
    TYPE unit
    SOURCES ${UNIT_TESTS_SOURCES}
    DEPENDENCIES client_comm
)

set(INTEGRATION_TESTS_SOURCES
    integration/main.cpp
)

client_add_test(
    TARGET client_comm_integration
    MODULE_NAME comm
    TYPE integration
    SOURCES ${INTEGRATION_TESTS_SOURCES}
    DEPENDENCIES client_comm
)

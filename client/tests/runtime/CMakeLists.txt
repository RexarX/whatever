set(UNIT_TESTS_SOURCES
    # App (runtime) module tests
    unit/app/app.cpp
    unit/app/app_return_code.cpp
    unit/app/camera.cpp
    unit/app/face_data.cpp
    unit/app/face_tracker.cpp
    unit/app/frame.cpp
    # TODO: These need include fixes
    # unit/app/gui_window.cpp
    unit/app/model_config.cpp

    unit/main.cpp
)

client_add_test(
    TARGET client_runtime_unit
    MODULE_NAME runtime
    TYPE unit
    SOURCES ${UNIT_TESTS_SOURCES}
    DEPENDENCIES client_runtime
)

set(INTEGRATION_TESTS_SOURCES
    integration/app_integration.cpp

    integration/main.cpp
)

client_add_test(
    TARGET client_runtime_integration
    MODULE_NAME runtime
    TYPE integration
    SOURCES ${INTEGRATION_TESTS_SOURCES}
    DEPENDENCIES client_runtime
)

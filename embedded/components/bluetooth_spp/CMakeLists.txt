# ESP-IDF component for Bluetooth SPP communication

idf_component_register(
    SRCS
        "bluetooth_spp.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        bt
        esp_common
        log
    PRIV_REQUIRES
        proto_nanopb
)

# C++23 standard for the component
set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

message(STATUS "Bluetooth SPP component configured")

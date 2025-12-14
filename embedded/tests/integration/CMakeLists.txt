# Integration Tests

# Example integration test
# embedded_add_integration_test(
#     NAME example_integration_test
#     SOURCES
#         example_integration_test.cpp
#     LIBRARIES
#         # Add any libraries needed for testing
#     INCLUDE_DIRS
#         ${EMBEDDED_ROOT_DIR}/main
#         ${EMBEDDED_ROOT_DIR}/include
#     MODULE core
# )

# Note: Add your integration tests here using the embedded_add_integration_test() function
# Integration tests typically test interactions between multiple components
#
# Example structure:
#
# embedded_add_integration_test(
#     NAME communication_protocol_test
#     SOURCES
#         communication_protocol_test.cpp
#         ${EMBEDDED_ROOT_DIR}/main/bluetooth_comm.cpp
#         ${EMBEDDED_ROOT_DIR}/main/protocol_handler.cpp
#     INCLUDE_DIRS
#         ${EMBEDDED_ROOT_DIR}/main
#         ${EMBEDDED_ROOT_DIR}/components/nanopb
#     MODULE communication
# )

message(STATUS "Integration tests directory configured (add tests in this CMakeLists.txt)")
